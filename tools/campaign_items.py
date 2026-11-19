"""Activate the White Mountain item routes on a prepared campaign copy."""

import json
import re

from audit_ipa_versions import decode, parts_records, table

INT32_MAX = 2**31 - 1


def read_rows(path):
    return decode(path.read_bytes()).decode('utf-8-sig').splitlines()


def render_rows(rows):
    return ('\n'.join(rows) + '\n').encode('utf-8')


def item_rows(directory, rows, definition):
    columns = [cell.strip() for cell in rows[0].split('\t')]
    if columns != ['Code', 'Icon', 'Type', 'Attr', 'Lv', 'Price', 'Param1',
                   'Param2', 'Param3', 'strParam', 'Name']:
        raise ValueError('Unexpected campaign ItemInfo columns')
    activate = set(definition['activate'])
    buy = set(definition['buy'])
    levels = definition['itemLevels']
    drop_levels = set(definition['dropLevelFromShop'])
    pickers = definition['pickers']
    template = definition['pickerTemplate']
    if activate & buy or levels.keys() - activate or (activate | buy | {template}) & pickers.keys():
        raise ValueError('Overlapping campaign ItemInfo edits')
    if drop_levels - (activate | buy):
        raise ValueError('Campaign drop levels require a campaign shop route')
    expected = activate | buy | {template}
    parts = set(parts_records((directory / 'PartsInfo.mcb').read_bytes()))
    weapons = {row['Parts']: row for row in table((directory / 'WeaponInfo.tble').read_bytes(), True)[0]}
    machines = {row['MachName']: row for row in table((directory / 'MachList.tble').read_bytes(), True)[0]}
    result = [rows[0] + '\tDropLv']
    visited = set()
    for line in rows[1:]:
        commented = line.startswith('//')
        fields = (line[2:] if commented else line).split('\t')
        code = fields[0].strip()
        if code in pickers:
            raise ValueError(f'Duplicate campaign item: {code}')
        if code not in expected:
            result.append(line)
            continue
        if code in visited or commented != (code in activate):
            raise ValueError(f'Unexpected source state for campaign item: {code}')
        visited.add(code)
        if code == '10318' and len(fields) == len(columns) + 1 and not fields[4]:
            # The dormant original LS-L21 row has an extra tab before Lv.
            del fields[4]
        if len(fields) != len(columns):
            raise ValueError(f'Unexpected ItemInfo field count: {code}')
        source = dict(zip(columns, fields))
        if code == template:
            if (source['Type'].strip(), source['Attr'].strip()) != ('Bot', 'BSC'):
                raise ValueError(f'Unexpected picker template: {template}')
            result.append(line)
            for picker_code, picker in pickers.items():
                machine = machines.get(picker['model'])
                if (not re.fullmatch(r'[0-9]{5}', picker_code) or not machine or
                        (machine['Type'], machine['Refer']) != ('PLANE', 'BOT_P')):
                    raise ValueError(f'Unresolved campaign picker: {picker_code}')
                body = machine['BaseParts']
                if (body not in parts or not (directory / f'{body}.png').is_file() or
                        not (directory / f'Icon{picker_code}.png').is_file()):
                    raise ValueError(f'Missing campaign picker body or icon: {picker_code}')
                new = dict(source, Code=picker_code, Icon=picker_code,
                           strParam=picker['model'], Name=picker['name'])
                result.append('\t'.join(new.values()))
            continue

        if source['Type'].strip() == 'PtWpn':
            weapon = source['strParam'].strip()
            if weapon not in parts or weapon not in weapons or not (directory / f'{weapon}.png').is_file():
                raise ValueError(f'Unresolved campaign weapon: {code}')
        else:
            weapon = source['strParam'].strip() + '_01'
            if source['Type'].strip() != 'SpAtk' or weapon not in weapons:
                raise ValueError(f'Unresolved campaign special attack: {code}')
        sound = weapons[weapon]['FireSound']
        icon = int(source['Icon'])
        if not (directory / f'{sound}.wav').is_file() or not (directory / f'Icon{icon:05}.png').is_file():
            raise ValueError(f'Missing campaign weapon sound or icon: {code}')
        if code in buy:
            if source['Attr'].strip() != 'S':
                raise ValueError(f'Campaign purchase expects an original sell-only item: {code}')
            source['Attr'] = 'B' + source['Attr']
        if code in levels:
            level = levels[code]
            if type(level) is not int or not 0 <= level <= 99:
                raise ValueError(f'Invalid campaign shop level: {code}')
            source['Lv'] = str(level)
        if code in drop_levels:
            level = int(source['Lv'])
            if not 0 <= level <= 99:
                raise ValueError(f'Invalid campaign source item level: {code}')
            source['DropLv'] = str(max(0, level - 7))
        result.append('\t'.join(source.values()))
    if visited != expected:
        raise ValueError(f'Missing campaign item rows: {sorted(expected - visited)}')
    return result


def weight_pairs(weights, known_items, existing_total=0):
    if not weights:
        raise ValueError('Empty campaign ItemRegen weights')
    for code, weight in weights.items():
        if code not in known_items or type(weight) is not int or not 1 <= weight <= INT32_MAX:
            raise ValueError(f'Invalid campaign loot item/weight: {code} {weight}')
    if sum(weights.values()) + existing_total > INT32_MAX:
        raise ValueError('Campaign ItemRegen total exceeds the original signed 32-bit range')
    return ' '.join(f'{code} {weight}' for code, weight in weights.items())


def drop_rows(rows, definition, known_items):
    if [cell.strip() for cell in rows[0].split('\t')] != ['Name', 'Min', 'Max', 'Regen']:
        raise ValueError('Unexpected campaign ItemRegen columns')
    extensions = definition['extendDrops']
    groups = definition['newDropGroups']
    if extensions.keys() & groups.keys():
        raise ValueError('Overlapping campaign ItemRegen groups')
    result = [rows[0]]
    visited = set()
    for line in rows[1:]:
        name = line.split('\t', 1)[0].strip()
        if name in groups:
            raise ValueError(f'Duplicate campaign ItemRegen group: {name}')
        if name not in extensions:
            result.append(line)
            continue
        fields = line.rstrip().split('\t')
        if name in visited or len(fields) != 4:
            raise ValueError(f'Unexpected ItemRegen source row: {name}')
        existing = fields[3].split()
        if len(existing) % 2 != 0 or set(existing[::2]) & extensions[name].keys():
            raise ValueError(f'Duplicate campaign ItemRegen items: {name}')
        visited.add(name)
        original_total = sum(map(int, existing[1::2]))
        result.append(line.rstrip() + ' ' + weight_pairs(extensions[name], known_items, original_total))
    if visited != extensions.keys():
        raise ValueError(f'Missing campaign ItemRegen groups: {sorted(extensions.keys() - visited)}')
    for name, group in groups.items():
        minimum, maximum = group['count']
        if any(type(value) is not int for value in (minimum, maximum)) or not 0 <= minimum <= maximum < INT32_MAX:
            raise ValueError(f'Invalid campaign loot count: {name}')
        result.append(f'{name}\t{minimum}\t{maximum}\t' + weight_pairs(group['weights'], known_items))
    return result


def apply_campaign_items(directory, definition_path):
    definition = json.loads(definition_path.read_text(encoding='utf-8'))
    items = directory / 'ItemInfo.tble'
    drops = directory / 'ItemRegen.tble'
    items_data = render_rows(item_rows(directory, read_rows(items), definition))
    known = {row['Code'] for row in table(items_data, True)[0]}
    drops_data = render_rows(drop_rows(read_rows(drops), definition, known))
    items.write_bytes(items_data)
    drops.write_bytes(drops_data)
    return items.name, drops.name
