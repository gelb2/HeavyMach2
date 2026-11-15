"""Read approved original packages locally; never extract or upload them."""
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

APPROVED={'ipad':('7eeb36590b3c19a28741e21671379029d9023864ceb241a5b8560eef045112d2','HM2HD.app'),
          'iphone':('23ff9c74c7d890cc6f28952b4845f6c616bae4bb7207a393f80ed9ed76062d32','HeavyMach2.app')}

def image_size(data):
    if data[:8]==b'\x89PNG\r\n\x1a\n':
        at=8
        while at+12<=len(data):
            size=struct.unpack_from('>I',data,at)[0]
            if at+size+12>len(data):break
            if data[at+4:at+8]==b'IHDR':return list(struct.unpack_from('>II',data,at+8))
            at+=size+12
    if data[:2]==b'\xff\xd8':
        at=2
        while at+4<len(data):
            if data[at]!=255:raise ValueError('Invalid JPEG segment')
            while data[at]==255:at+=1
            kind=data[at];at+=1
            if kind in (1,0xd8,0xd9) or 0xd0<=kind<=0xd7:continue
            size=struct.unpack_from('>H',data,at)[0]
            if kind in (0xc0,0xc1,0xc2,0xc3,0xc5,0xc6,0xc7,0xc9,0xca,0xcb,0xcd,0xce,0xcf):
                height,width=struct.unpack_from('>HH',data,at+3);return [width,height]
            if size<2:break
            at+=size
    raise ValueError('Unsupported original image header')

class OriginalCatalog:
    def __init__(self,root):
        self.root=Path(root);provenance=json.loads((self.root/'docs/source-provenance.json').read_text());self.records={};self.bases={};self.base_refs={}
        for profile,(expected,app) in APPROVED.items():
            matches=[x for x in provenance['files'] if x['sha256']==expected]
            if len(matches)!=1:raise ValueError('Unapproved original catalog '+profile)
            source=Path(provenance['source_root'])/matches[0]['relative_path']
            with source.open('rb') as stream:
                if hashlib.file_digest(stream,'sha256').hexdigest()!=expected:raise ValueError('Original package hash differs')
            table_names=set()
            with zipfile.ZipFile(source) as archive:
                for entry in archive.infolist():
                    path=Path(entry.filename)
                    if len(path.parts)!=3 or path.parts[:2]!=('Payload',app):continue
                    name=path.name
                    if path.suffix.lower() not in {'.png','.jpg','.tble'}:continue
                    data=archive.read(entry);key=('PhoneMenu/' if profile=='iphone' else '')+name
                    if key in self.records:raise ValueError('Duplicate original resource')
                    record={'sha256':hashlib.sha256(data).hexdigest(),'profile':profile}
                    if path.suffix.lower() in {'.png','.jpg'}:record['pixels']=image_size(data)
                    self.records[key]=record
                    if re.fullmatch('Base[0-9]+\\.tble',name):
                        table_names.add(path.stem)
                        from common_decode import table
                        rows=table(self.root,data)
                        self.base_refs[(profile,path.stem)]={(r['type'],r['id']):('PhoneMenu/' if profile=='iphone' else '')+r['image'] for r in rows}
            self.bases[profile]=table_names

    def verify_prepared_bases(self):
        base=self.root/'.local/prepared/HD';required=self.bases['ipad']
        if not required or self.bases['iphone']!=required:raise ValueError('Original base sets differ; requires explicit mapping')
        for profile,names in self.bases.items():
            directory=base/'PhoneMenu' if profile=='iphone' else base
            actual={p.stem for p in directory.glob('Base*.tble')}
            if actual!=names:raise ValueError('Prepared base tables missing or extra: '+profile)
            for name in names:
                resource=('PhoneMenu/' if profile=='iphone' else '')+name+'.tble';path=base/resource
                if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=self.records[resource]['sha256']:raise ValueError('Prepared base data differs from original')
        return sorted(required)

    def backdrop(self,profile,base,screen):
        if (profile,base) not in self.base_refs:raise ValueError('Unknown original base')
        if screen=='hub':return self.base_refs[(profile,base)][('Image',0)]
        if screen in {'command','shop'}:return ('PhoneMenu/' if profile=='iphone' else '')+('CmdCenterBG' if screen=='command' else 'ShopBG')+('.jpg' if profile=='iphone' else '.png')
        raise ValueError('Unknown menu screen')

def original_catalog(root):return OriginalCatalog(root)
