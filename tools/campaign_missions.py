"""Write original-format quest rows for the authored White Mountain campaign."""

import re

from audit_ipa_versions import table


def apply_campaign_missions(directory, definition, maps, append_rows):
    original_missions = directory/'MissionList.tble'
    existing = {row['MissionId'] for row in table(original_missions.read_bytes())[0]}
    machines = {row['MachName']: row for row in table((directory/'MachList.tble').read_bytes())[0]}
    mission_rows = []
    reports = []
    for mission in definition['missions']:
        mission_id = mission['id']
        enemy = mission['model']
        stage = mission['map']
        previous = mission['previous']
        level = mission['level']
        if not re.fullmatch(r'M\d{6}', mission_id) or mission_id in existing:
            raise ValueError(f'Duplicate or invalid campaign mission: {mission_id}')
        if previous not in existing or stage not in maps or not 0 <= level <= 99:
            raise ValueError(f'Invalid mission prerequisite, destination or level: {mission_id}')
        if not enemy.startswith('Boss_') or machines[enemy]['Atk'] != '1':
            raise ValueError(f'Missing armed campaign boss: {enemy}')
        x, y = mission['at']
        if type(x) is not int or type(y) is not int or max(abs(x), abs(y)) > 400:
            raise ValueError(f'Invalid campaign boss position: {stage} {(x,y)}')
        radius = mission['activationRadius']
        if type(radius) is not int or not 1 <= radius <= 1024:
            raise ValueError(f'Invalid campaign boss approach radius: {mission_id}')
        hp = int(machines[enemy]['HP'])
        # The last original boss contract awards roughly 3.33 Cr and XP per HP.
        reward = hp*3
        mission_rows.append((mission_id, 'Base004', 'Base004', level, previous))
        for language in ('us', 'kr'):
            name, comment = mission[language]
            path = directory/f'{mission_id}_{language}.tble'
            if path.exists() or any(c in name+comment for c in '\t\r\n'):
                raise ValueError(f'Invalid or duplicate campaign mission text: {path.name}')
            rows = (
                ('Category','Value'), ('Name',name),
                ('AcptComment',comment), ('AcceptAct',f'placeSpawner {enemy} {stage} {x} {y} {radius}'),
                ('Require',f'killAt {enemy} 1 {stage}'), ('CompleteInf','return'),
                ('CompleteAct',f'giveMoney {reward}'), ('CompleteAct',f'giveExp {reward}'),
            )
            path.write_text(''.join(f'{kind}\t{text}\n' for kind,text in rows), encoding='utf-8')
        existing.add(mission_id)
        reports.append(dict(id=mission_id,map=stage,model=enemy,hitPoints=hp,level=level,
                            previous=previous,credits=reward,experience=reward,activationRadius=radius))
    append_rows(original_missions, mission_rows)
    return reports
