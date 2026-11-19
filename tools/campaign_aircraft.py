"""Bind the three dormant aircraft to authored weapons in the campaign copy."""

from dataclasses import replace
import math
import struct

from audit_ipa_versions import decode, table

DORMANT_MODELS = {'E_Plane_01', 'E_Plane_02', 'E_Plane_05'}


def name_of(data):
    return data[:24].split(b'\0', 1)[0].decode('ascii')


def part_id(name):
    data = name.encode('ascii')
    if len(data) >= 24 or not data or any(not (chr(c).isalnum() or c == 95) for c in data):
        raise ValueError(f'Invalid aircraft part ID: {name}')
    return data.ljust(24, b'\0')


def records(root, kind):
    result = {name_of(node.data): node for node in root.children if node.kind == kind}
    if len(result) != len(root.children):
        raise ValueError(f'Duplicate or unexpected aircraft catalog node type: {kind}')
    return result


def add_sockets(part, sockets):
    count = part.data[26]
    if len(part.data) != 27 + count * 8 or count + len(sockets) > 255:
        raise ValueError(f'Invalid sockets for {name_of(part.data)}')
    if any(not math.isfinite(x) or not math.isfinite(y) for x, y in sockets):
        raise ValueError(f'Nonfinite sockets for {name_of(part.data)}')
    part.data = part.data[:26] + bytes([count+len(sockets)]) + part.data[27:]
    part.data += b''.join(struct.pack('<ff', x, y) for x, y in sockets)
    part.declared = 0


def mount_weapon(model, body, pod, definition):
    sockets = definition['bodySockets']
    if sockets:
        add_sockets(body, sockets)
    body_node = model.children[0]
    weapon_name = part_id(definition['weapon']['part'])
    mount_sockets = definition['mountSockets']
    if not 1 <= len(mount_sockets) <= 4 or len(set(mount_sockets)) != len(mount_sockets):
        raise ValueError(f'Invalid aircraft weapon sockets: {name_of(model.data)}')
    by_socket = {child.data[24]: child for child in body_node.children}
    for socket in mount_sockets:
        if type(socket) is not int or not 0 < socket < body.data[26]:
            raise ValueError(f'Unavailable socket {socket} in {name_of(model.data)}')
        node = by_socket.get(socket)
        if node is None:
            node = replace(body_node, data=weapon_name + bytes([socket, 0]), children=[], declared=0)
            body_node.children.append(node)
            by_socket[socket] = node
        elif name_of(node.data) != pod:
            raise ValueError(f'Occupied aircraft weapon socket {socket} in {name_of(model.data)}')
        else:
            node.data = weapon_name + node.data[24:]
    body_node.declared = model.declared = 0


def update_machine_table(data):
    original = decode(data).decode('utf-8-sig')
    lines = original.splitlines(keepends=True)
    columns = [cell.strip() for cell in lines[0].rstrip().split('\t')]
    kind_column, attack_column = columns.index('Type'), columns.index('Atk')
    changed = set()
    for i, line in enumerate(lines[1:], 1):
        row = line.rstrip('\r\n')
        cells = row.split('\t')
        name = cells[0].strip()
        if name not in DORMANT_MODELS:
            continue
        has_extra_columns = any(cell.strip() for cell in cells[len(columns):])
        if (name in changed or len(cells) < len(columns) or has_extra_columns or
                cells[kind_column].strip() != 'PLANE' or cells[attack_column].strip() != '0'):
            raise ValueError(f'Invalid original dormant aircraft row: {name}')
        cells[attack_column] = '1'
        lines[i] = '\t'.join(cells) + line[len(row):]
        changed.add(name)
    if changed != DORMANT_MODELS:
        raise ValueError(f'Original aircraft rows missing: {DORMANT_MODELS - changed}')
    return ''.join(lines).encode('utf-8')


def weapon_row(weapons, definition, directory):
    donor = weapons[definition['donor']]
    overrides = definition['fields']
    if set(overrides) - {'Dmg', 'Range', 'ShtRng', 'ShtCnt', 'RldDly', 'AimRt'}:
        raise ValueError(f'Unknown aircraft weapon fields: {definition["part"]}')
    row = {**donor, **overrides, 'Parts': definition['part']}
    if row['Type'] not in {'Bullet', 'Missile'} or int(row['Range']) <= 0 or int(row['ShtCnt']) != 1 or float(row['RldDly']) < 2:
        raise ValueError(f'Invalid aircraft damage radius or shot limit: {row["Parts"]}')
    assets = [row['FireSound'] + '.wav', 'Blt_' + row['BltTile'] + '.png']
    if row['EffType'] != '-':
        assets.append(row['EffTile'] + '.png')
    if row['ExplodeEff'] != '-':
        assets.append('Efx_' + row['ExplodeEff'] + '.png')
    for name in assets:
        if not (directory / name).is_file():
            raise ValueError(f'Missing aircraft donor asset: {name}')
    return '\t'.join(str(row[key]) for key in donor)


def extend_aircraft(directory, definition, *, parse_chunk):
    """Return changed and created resource names for the campaign manifest."""
    aircraft = definition['aircraft']
    pod = definition['pod']
    if aircraft.keys() != DORMANT_MODELS or len(definition['muzzle']) != 2 or not any(definition['muzzle']):
        raise ValueError('Expected three dormant aircraft and a nonzero muzzle')
    parts_path = directory / 'PartsInfo.mcb'
    models_path = directory / 'MachInfo.mcb'
    machines_path = directory / 'MachList.tble'
    weapons_path = directory / 'WeaponInfo.tble'
    part_root = parse_chunk(parts_path.read_bytes())
    model_root = parse_chunk(models_path.read_bytes(), root_kind=2)
    parts = records(part_root, 1)
    models = records(model_root, 3)
    weapons = {row['Parts']: row for row in table(weapons_path.read_bytes(), strict_keys=True)[0]}
    pod_part = parts[pod]
    if pod_part.data[24:27] != bytes([4, 1, 1]) or pod in weapons:
        raise ValueError(f'Expected the original display-only weapon pod: {pod}')
    pod_image = (directory / (pod+'.png')).read_bytes()
    aliases = {}
    new_rows = []
    for name, record in aircraft.items():
        model = models[name]
        weapon = record['weapon']
        alias = weapon['part']
        if alias in weapons or alias in parts or (directory / (alias+'.png')).exists():
            raise ValueError(f'Duplicate aircraft weapon part: {alias}')
        if len(model.children) != 1:
            raise ValueError(f'Expected a single root aircraft part: {name}')
        body = parts[name_of(model.children[0].data)]
        if body.data[24] != 0:
            raise ValueError(f'Expected one main aircraft body: {name}')
        new_part = replace(pod_part, data=part_id(alias) + pod_part.data[24:], declared=0)
        add_sockets(new_part, [definition['muzzle']])
        parts[alias] = new_part
        mount_weapon(model, body, pod, record)
        part_root.children.append(new_part)
        new_rows.append(weapon_row(weapons, weapon, directory))
        aliases[alias+'.png'] = pod_image

    part_root.declared = model_root.declared = 0
    updated_machines = update_machine_table(machines_path.read_bytes())
    original_weapons = decode(weapons_path.read_bytes()).decode('utf-8-sig')
    updated_weapons = original_weapons.rstrip('\r\n') + '\n' + '\n'.join(new_rows) + '\n'
    parts_path.write_bytes(part_root.encode())
    models_path.write_bytes(model_root.encode())
    machines_path.write_bytes(updated_machines)
    weapons_path.write_text(updated_weapons, encoding='utf-8')
    for name, pixels in aliases.items():
        (directory / name).write_bytes(pixels)
    return [parts_path.name, models_path.name, machines_path.name, weapons_path.name, *aliases]
