use std::{
    fs::{self, File},
    io::{self, ErrorKind, Read, Write},
    path::Path,
};

const SAVE_SIZE: usize = 28_204;
const HUD_LIMIT: usize = 65_536;

#[derive(Clone, Copy)]
pub enum Key {
    Save,
    Backup,
    CompactHud,
    RegularHud,
}

impl Key {
    pub fn parse(key: &str) -> io::Result<Self> {
        match key {
            "save" => Ok(Self::Save),
            "backup" => Ok(Self::Backup),
            "hud/compact" => Ok(Self::CompactHud),
            "hud/regular" => Ok(Self::RegularHud),
            _ => Err(invalid("Unknown storage key")),
        }
    }

    fn filename(self) -> &'static str {
        match self {
            Self::Save => "HeavyMach2.sav",
            Self::Backup => "BeforeRecovery.sav",
            Self::CompactHud => "HUDLayoutV1-compact.json",
            Self::RegularHud => "HUDLayoutV1-regular.json",
        }
    }

    fn max_size(self) -> usize {
        match self {
            Self::Save | Self::Backup => SAVE_SIZE,
            Self::CompactHud | Self::RegularHud => HUD_LIMIT,
        }
    }

    fn check(self, size: usize) -> io::Result<()> {
        let valid = match self {
            Self::Save | Self::Backup => size == SAVE_SIZE,
            Self::CompactHud | Self::RegularHud => (1..=HUD_LIMIT).contains(&size),
        };
        if valid {
            Ok(())
        } else {
            Err(invalid("Unexpected storage record size"))
        }
    }
}

fn invalid(message: &'static str) -> io::Error {
    io::Error::new(ErrorKind::InvalidData, message)
}

pub fn read(root: &Path, key: Key) -> io::Result<Option<Vec<u8>>> {
    let path = root.join(key.filename());
    let info = match fs::symlink_metadata(&path) {
        Ok(info) => info,
        Err(error) if error.kind() == ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(error),
    };
    if !info.is_file() {
        return Err(invalid("Storage record must be a regular file"));
    }
    key.check(info.len().try_into().unwrap_or(usize::MAX))?;
    let mut bytes = Vec::new();
    File::open(path)?
        .take(key.max_size() as u64 + 1)
        .read_to_end(&mut bytes)?;
    key.check(bytes.len())?;
    Ok(Some(bytes))
}

/// Each call commits one record. A restore writes and verifies the separate
/// backup before requesting replacement of the current save.
pub fn write(root: &Path, key: Key, bytes: &[u8]) -> io::Result<()> {
    key.check(bytes.len())?;
    fs::create_dir_all(root)?;
    read(root, key)?; // Preserve a damaged or non-regular existing record.
    let mut pending = tempfile::NamedTempFile::new_in(root)?;
    pending.write_all(bytes)?;
    pending.as_file().sync_all()?;
    pending.persist(root.join(key.filename()))?;
    File::open(root)?.sync_all()?;
    if read(root, key)?.as_deref() != Some(bytes) {
        return Err(invalid("Could not verify the committed storage record"));
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn recovery_keeps_previous_save_after_successful_replace() {
        let dir = tempfile::tempdir().unwrap();
        let root = dir.path();
        let first = vec![5; SAVE_SIZE];
        let restored = vec![6; SAVE_SIZE];
        assert!(read(root, Key::Save).unwrap().is_none());
        write(root, Key::Save, &first).unwrap();
        write(root, Key::Backup, &first).unwrap();
        write(root, Key::Save, &restored).unwrap();
        assert_eq!(read(root, Key::Save).unwrap(), Some(restored));
        assert_eq!(read(root, Key::Backup).unwrap(), Some(first));
    }

    #[test]
    fn interrupted_or_invalid_replacement_retains_both_records() {
        let dir = tempfile::tempdir().unwrap();
        let root = dir.path();
        let initial = vec![2; SAVE_SIZE];
        write(root, Key::Save, &initial).unwrap();
        write(root, Key::Backup, &initial).unwrap();
        fs::write(root.join(".uncommitted"), vec![3; SAVE_SIZE / 2]).unwrap();
        assert!(write(root, Key::Save, &[3; 17]).is_err());
        assert_eq!(
            read(root, Key::Save).unwrap().as_deref(),
            Some(initial.as_slice())
        );
        assert_eq!(read(root, Key::Backup).unwrap(), Some(initial));
    }

    #[test]
    fn rejects_unsafe_keys_and_keeps_incompatible_local_file() {
        let dir = tempfile::tempdir().unwrap();
        let root = dir.path();
        assert!(Key::parse("hud/../../profile").is_err());
        let damaged = root.join(Key::Save.filename());
        fs::write(&damaged, b"keep these bytes").unwrap();
        assert!(read(root, Key::Save).is_err());
        assert!(write(root, Key::Save, &vec![1; SAVE_SIZE]).is_err());
        assert_eq!(fs::read(damaged).unwrap(), b"keep these bytes");
    }

    #[test]
    fn hud_profiles_are_separate_and_bounded() {
        let dir = tempfile::tempdir().unwrap();
        write(dir.path(), Key::CompactHud, b"compact").unwrap();
        write(dir.path(), Key::RegularHud, b"regular").unwrap();
        assert!(write(dir.path(), Key::CompactHud, &[]).is_err());
        assert!(write(dir.path(), Key::RegularHud, &vec![1; HUD_LIMIT + 1]).is_err());
        assert_eq!(
            read(dir.path(), Key::CompactHud).unwrap().unwrap(),
            b"compact"
        );
        assert_eq!(
            read(dir.path(), Key::RegularHud).unwrap().unwrap(),
            b"regular"
        );
    }
}
