#!/usr/bin/env python3
"""Reproduce metadata-only UI evidence; original strings and bodies stay local."""
from pathlib import Path
import argparse, hashlib, json, re, subprocess

REPO = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description='Index original UI metadata and reviewed contracts without exporting function bodies')
parser.add_argument('--analysis-root', type=Path, default=Path('/Users/sokol/Documents/Codex/HeavyMach2/Analysis'))
parser.add_argument('--output', type=Path, default=REPO / '.local/evidence/iphone-universal-ui-followup-20261113')
args = parser.parse_args()
ANALYSIS = args.analysis_root
PHONE = ANALYSIS / 'Phone201/universal-audit-20261113/full-export'
HD = ANALYSIS / 'HD103'
OUT = args.output.resolve()
if not OUT.is_relative_to(REPO / '.local'):
    parser.error('Evidence output must remain inside this repository .local space')
OUT.mkdir(parents=True, exist_ok=True)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
pm = json.loads((PHONE/'export-manifest.json').read_text())
hm = json.loads((HD/'method-evidence/export-manifest.json').read_text())
hn = json.loads((HD/'native-evidence/export-manifest.json').read_text())
layout = json.loads((HD/'objc-layouts.json').read_text())
cfs_path = REPO/'.local/evidence/iphone-universal-integration-20261113/hd-cfstrings.json'
cfs = {x['address']:x['text'] for x in json.loads(cfs_path.read_text())['constants']}
allowed_resource = lambda t: bool(re.search(r'\.(png|jpe?g|wav|mp3)$',t,re.I)) and len(t)<160
phone_methods={x['address']:x for x in pm['methods']}
hd_methods={x['address']:(x,HD/'method-evidence') for x in hm['methods']}
hd_methods.update({x['address']:(x,HD/'native-evidence') for x in hn['methods']})

def evidence(version,address):
    if version=='phone201' and address not in phone_methods:
        walk=ANALYSIS / 'Phone201/iphone-universal-followup-20261113/0x39e14.instruction-walk.asm'
        if address=='0x39e14' and walk.exists():
            return {'version':version,'address':address,'name':'GuiCommandCenter tick entry; vtable and native instruction walk','resources':[],'selector_index':[],'exports':[{'kind':'instruction-walk','path':str(walk),'bytes':walk.stat().st_size,'sha256':sha(walk),'evidence_id':'ev_9eb148fd63bbac09090ee6e386172bf2441398c798d072c89b4eb7a4d3134028'}],'meaning_status':'selected-entry-walk-reviewed','limitation':'기존 1077개 함수 몸체 분모 밖 진입점을 추가 추적했다. 명령의 관측 순서이며 정렬된 전체 디스어셈블리로 오인하지 않는다.'}
        return {'version':version,'address':address,'name':'metadata-only entry','resources':[],'selector_index':[],'exports':[],'meaning_status':'metadata-entry-body-not-in-cache','limitation':'RTTI/vtable 후보다. 본문 동등성과 동작은 확인하지 못했다.'}
    if version=='phone201':m,root=phone_methods[address],PHONE
    else:m,root=hd_methods[address]
    text=(root/'functions'/f'{address}.pseudo.c').read_text()
    files=[]
    for kind,ext in [('pseudo','pseudo.c'),('assembly','asm')]:
        f=root/'functions'/f'{address}.{ext}';meta=m['exports'][kind];actual=sha(f)
        if meta.get('sha256') and meta['sha256']!=actual:raise ValueError(str(f))
        files.append({'kind':kind,'path':str(f),'bytes':f.stat().st_size,'sha256':actual,'evidence_id':meta.get('evidence_id')})
    names={s for s in re.findall(r'"([^"\n]*)"',text) if allowed_resource(s)}
    for address_ in re.findall(r'\b0x[0-9a-f]+\b',text):
        name=cfs.get(address_)
        if name and allowed_resource(name):names.add(name)
    selectors=sorted(set(re.findall(r'"([A-Za-z_][A-Za-z0-9_]*(?::[A-Za-z0-9_]*)*:?)"',text)))
    selectors=[s for s in selectors if ':' in s or s in ('remove','refresh','tick','screenPosX','screenPosY','worldPosX','worldPosY','calcWorldPos','saveDataFile','closeDialog')]
    return {'version':version,'address':address,'name':m.get('name',m.get('owner','')+'::'+m.get('selector','')),'resources':sorted(names),'selector_index':selectors,'exports':files,'meaning_status':'syntax-index-only'}

# This scans the complete declared UI class/method scope. It is an address and
# resource index, not an automatic claim of semantic equivalence.
hd_ui_classes=[c for c in layout['classes'] if c['name'].startswith(('Gui','Dlg','Wnd')) or c['name']=='GameMain']
hd_ui_addresses=sorted({m['address'] for c in hd_ui_classes for m in c['methods']},key=lambda s:int(s,16))
phone_ui_ranges=[(0x24cb8,0x46840),(0x55e0,0x7958),(0x5086c,0x50e84)]
phone_ui_addresses=sorted([a for a in phone_methods if any(lo<=int(a,16)<=hi for lo,hi in phone_ui_ranges)],key=lambda s:int(s,16))
index=[evidence('phone201',a) for a in phone_ui_addresses]+[evidence('hd103',a) for a in hd_ui_addresses]
clip=[]
for version,methods,root in [('phone201',pm['methods'],PHONE),('hd103',hm['methods'],HD/'method-evidence'),('hd103-native',hn['methods'],HD/'native-evidence')]:
    for m in methods:
        f=root/'functions'/f"{m['address']}.pseudo.c"
        if re.search(r'glScissor|glEnable\(0xc11|glDisable\(0xc11|setClip|setScissor|clipRect',f.read_text()):clip.append({'version':version,'address':m['address'],'path':str(f),'sha256':sha(f)})

groups=[]
def group(id,title,p,h,fact,parent,hit,callback,source,decision,status='confirmed-selected-contract'):
    groups.append({'id':id,'title':title,'status':status,'phone':p,'hd':h,'facts':fact,'parent_and_visibility':parent,'clip_and_hit':hit,'callback_and_transition':callback,'current_cpp':source,'universal_design':decision})

group('inventory-dialog','인벤토리 창',['0x26240','0x26a38'],['0x20b0c','0x2113c'],
 '일반판 InvenBG는400×256, HD는768×512. 일반판은 장비와 인벤토리 자식을 만들고 HD는 BOT 자식·상점모드 분기도 포함한다.',
 '일반판 장비(-111,50), 인벤(-5,60). HD 장비(-196,76), 인벤(24,120), BOT(-58,78). HD 창은 필드 중앙 또는 상점의(0,-200) 자식이다.',
 '닫기6092·정렬6093과 페이지·칸 버튼은 창/하위 격자에 속한다. 창 그림의 확대와 별개로 각 입력 영역을 연결해야 한다.',
 'HD Push/Move/Pop/Cancel를 구독해 페이지·선택·드래그·닫기를 처리한다. 일반판도 해당 문자열 이벤트를 분리한다.',
 ['Sources/Core/UI/Screens.cpp:566','Sources/Core/Session/GameSession.cpp:458'],
 'HD25개 인벤토리와 장비/BOT/창 관계를 공유한다. 상점 창의 부모 이동을 드래그 계산에도 적용한다.')
group('inventory-grid','인벤토리 격자·페이지',['0x31d2c','0x325f0','0x327a4','0x32828'],['0x3490c','0x345dc','0x34218'],
 '일반판5열×4행·간격40·페이지당20개. HD5×5·간격60·페이지당25개. 생성뿐 아니라 좌표→슬롯 계산에도20/25가 들어간다.',
 '칸 버튼이 격자 자식, 숫자·아이콘은 칸 자식이며 선택/드롭 강조는 격자 자식이다.',
 'HD 드롭은 (x-origin+30)/60, (y-origin-30)/-60이며 각각[0,5). 페이지 이동6090/6091은 별도 영역이다.',
 '칸6000..6024·페이지6090/6091. 페이지를 바꾸면 선택/상세를 정리한다.',
 ['Sources/Core/UI/Screens.cpp:517','Sources/Core/Session/GameSession.cpp:437','Sources/Core/Session/GameSession.cpp:458'],
 '폰에20개 규칙을 이식하지 않는다. 같은25개와 저장 인덱스를 유지하고 보기 영역 또는 배치를 바꾼다.')
group('equipment','장비6칸',['0x30864','0x319b4'],['0x33c5c','0x339a4'],
 '양쪽6칸과 종류11/12/10/10/13/14를 사용한다. 일반판 위치(0,0),(0,-56),(-37,-5),(37,-5),(37,-56),(-37,-56). HD는(0,0),(0,-74),(-56,-9),(56,-9),(56,-74),(-56,-74).',
 '장비 패널 자식 버튼6100..6105와 수치/강조를 함께 이동해야 한다.',
 '양쪽 드롭은 각 장비 중심±20의 엄격한 내부 비교다. 그려진 버튼의 크기와 드롭 영역은 구분한다.',
 '장비 종류에 맞는 대상에서만 드롭한다. 현재 C++는 단순 탭 장착을 추가하지 않는다.',
 ['Sources/Core/UI/Screens.cpp:541','Sources/Core/Session/GameSession.cpp:437'],
 'HD 장비 위치·종류·드롭 규칙을 공통 Layout 결과로 공유한다.')
group('bot-slots','BOT3칸',['0x2fb54','0x301a0'],['0x33300','0x3301c'],
 '양쪽3칸. 일반판 드롭 간격38, HD60이며 오른쪽 경계는<=1인 별도 규칙이다.',
 '정비/BOT 패널의 세로 자식이고 선택·외부드롭 강조도 같은 패널에 속한다.',
 'HD 드롭x∈[0,1], y∈[0,3). 일반판도 같은 범위 형태이나 단위38이다. 화면 바깥으로 보이지 않더라도 임의 터치 확장을 추가하지 않는다.',
 '6300..6302 선택과6310 해제. 필드에서 허용하지 않는 변경은 그대로 유지한다.',
 ['Sources/Core/UI/Screens.cpp:557','Sources/Core/Session/GameSession.cpp:437','Sources/Core/Session/GameSession.cpp:458'],
 'HD3칸과 권한을 유지하고 일반판38을 단순 HD 텍스처 배율로 해석하지 않는다.')
group('quick-slots','퀵슬롯5칸',['0x332a0','0x337b8','0x34040'],['0x35554','0x34e54','0x352dc'],
 '양쪽5칸. 일반판 드롭 단위44, HD57. 표시/버튼/드롭 영역은 각각 독립 소비자가 있다.',
 '일반판 HUD 배경 자식. HD GuiGame 자식(511,29), 숫자/아이콘은 칸 자식이다.',
 'HD 버튼은48×48이나 드롭 격자는57×57이다. 버튼 크기로 드롭 간격을 대체하면 안 된다.',
 '6200..6204 Push에서 일시정지/재고를 확인하고 현재 사용아이템이면 해제, 아니면 useItem 경로를 선택한다.',
 ['Sources/Core/UI/Screens.cpp:377','Sources/Core/Session/GameSession.cpp:437','Sources/Core/Session/GameSession.cpp:772'],
 '5칸·HD 사용조건을 공유하며 표시와 버튼 hit 및 드롭격자에 같은 배치 원점을 전달한다.')
group('shop','상점·상품페이지',['0x403b0','0x40f2c','0x4369c','0x415f0'],['0x30e4c','0x30260','0x2f21c'],
 '일반판은 상품4×4·간격40·16개/페이지, HD는6×4·간격60·24개/페이지. 일반판의 좌/우 범주 버튼과 HD의5개 직접 범주 버튼은 구조가 다르다.',
 '일반판 GuiShop 내 shop/equip 패널을 전환한다. HD는 상점·내장 DlgInven·거래/상세 자식을 만들고 정비는 별도 화면이다.',
 'HD 범주 hit80×48을 정한 뒤 선택상태별 scale을 바꾼다. 표시 scale과 입력 bound 재계산 시점을 보존한다.',
 'HD2104+intData 범주,2105/2106페이지,2200..2223선택,2102/2103구매/판매,2100복귀. 일반판의 같은 숫자에 역할을 대입하지 않는다.',
 ['Sources/Core/UI/Screens.cpp:590','Sources/Core/Session/GameSession.cpp:507'],
 'HD 상품/페이지 인덱스와 선택/재고/가격 조건을 공유한다. iPhone 배치 때문에 상품 데이터·개수를 줄이지 않는다.')
group('maintenance','정비 화면',['0x403b0','0x43328','0x415f0'],['0x2bb74','0x2c1f8'],
 '일반판은 GuiShop이 MaintenanceBG.jpg960×640과 ShopBG.jpg를 모드에 따라 선택한다. HD는 GuiMaintenance 별도 클래스와 MaintenanceBG.png480×320을 사용한다.',
 'HD 정비 중심(384,512) 아래 장비·인벤·BOT·정렬/해제/복귀·수치를 조립한다. 480×320 배경을 전체 HD 크기로 무조건 확장하지 않는다.',
 '장비/인벤/BOT 드롭 원점이 필드 인벤 창과 다르다. 별도 로직이 아니라 같은 위젯의 배치 데이터로 표현한다.',
 'HD 정비는 Push/Move/Pop/Cancel 경계에서 선택/드롭/해제/기지복귀를 처리한다.',
 ['Sources/Core/UI/Screens.cpp:581','Sources/Core/Session/GameSession.cpp:437','Sources/Core/Session/GameSession.cpp:458'],
 'HD 정비의 혼합 크기 자원을 그대로 쓰고 패널과 입력을 조립 단위로 배치한다.')
group('trade','거래 팝업',['0x3f4dc','0x415f0'],['0x31b20','0x2f21c'],
 '일반판 TradeItemBG512×512, HD512×256. 수량/가격/아이콘/텍스트와 증감·취소·구매/판매 버튼을 배경 자식으로 만든다.',
 'HD는 layer13의 QobButton 배경으로 아래 상점 입력을 막는다. 팝업이 보이면 원래 거래 gate를 우선한다.',
 'HD 배경 hit768×1024는 실제 배경 픽셀512×256과 다르다. 전체 입력 차단 영역을 그림 크기로 축소하면 안 된다.',
 '일반판 DEC2111/INC2112/CANCEL2108/BUY2109/SELL2110. HD DEC2110/INC2111/CANCEL2107/BUY2108/SELL2109. 기능으로 대응해야 한다.',
 ['Sources/Core/UI/Screens.cpp:638','Sources/Core/Session/GameSession.cpp:507','Sources/Core/Session/GameSession.cpp:714'],
 '공통 modal coverage는 유효 표시 영역을 덮고 자식 버튼은 HD 거래 의미를 유지한다. 가로 화면 빈 부분에서 뒤 버튼으로 입력이 새지 않아야 한다.')
group('item-detail','아이템 상세·팽창',['0x442c8'],['0x22400','0x2d208','0x31554','0x2065c','0x30888','0x2b770'],
 '일반판 ItemDesc512×256, HD256×256. 일반판 기본 상세 버튼2113, HD2112. HD 필드 인벤·정비·상점의 부모/애니메이션 위치가 다르다.',
 'HD 제목·설명·큰 파츠 그림이 상세 배경 자식이다. 초기scale.01과 가로→세로 팽창, 표시시점을 분리한다.',
 '상세 입력 영역206×128을 먼저 만들고 이후 표시scale을 바꾸는 순서다. 동적 생성 receiver가 기존 상점 버튼보다 우선한다.',
 'HD2112에서 상세를 닫는다. 무기/몸체/하체 아이콘·설명과 종류별 위치를 사용한다. 일반판의 전체동적팽창 세부 대응은 후보로 남긴다.',
 ['Sources/Core/UI/Screens.cpp:453','Sources/Core/UI/Screens.cpp:482','Sources/Core/UI/Screens.cpp:492'],
 'HD 애니메이션 상태를 보존하고 팝업 앵커/가독영역을 배치 결과에 연결한다. 화면높이를 직접 치환하지 않는다.','candidate-exact-animation-pairing')
group('airlift','에어리프트',['0x25524','0x24cb8','0x25404'],['0x1fbb0','0x1f5b8','0x1f838'],
 '일반판 생성5행, HD6행. 일반판 AirliftBG256×256, HD는512×64 상단+공통 중/하단 조립이다.',
 'HD 행은 상단 배경 자식, 확인/닫기는 하단 자식이다. 해금행 수로 상·중·하 높이를 조정한다.',
 'HD 행4002..4007, 확인4001, 닫기4000. 움직인 부모와 행/문자/선택 강조 hit를 같이 이동한다.',
 'Pop에서 행 선택·가격검사·차감·기지이동 요청. 닫기는 pause 해제. 결제/실제 화면교체 시점은 배치와 분리한다.',
 ['Sources/Core/UI/Screens.cpp:792','Sources/Core/Session/GameSession.cpp:748'],
 'HD6개 대상/해금조건을 유지하며 높이가 가로 유효영역을 넘으면 같은 자원 GPU 패널 안의 스크롤을 명시적 표시확장으로 설계한다.')
group('title','타이틀',['0x454f0','0x45cb8','0x45e8c'],['0x32340','0x3220c','0x327f4'],
 '일반판 배경480×320, HD768×1024. Main_Title/에피소드 원본 픽셀 크기도 서로2배 관계가 일정하지 않다. 입력 준비시간은 일반판11초, HD10초다.',
 '배경·큰 타이틀·메인 타이틀·에피소드·시작 안내가 개별 자식이다. HD에는 별도 Copyright 자식도 있다.',
 '타이틀은 전체화면 터치 receiver다. 유효게임영역과 바깥 안전영역 입력정책을 명시한다.',
 '끝 터치에서 준비 전이면 시작시간을 당기고 큰 타이틀을 숨김, 준비 후면 슬롯화면 요청. HUD 버튼으로 재구현하지 않는다.',
 ['Sources/Core/UI/Screens.cpp:356','Sources/Core/UI/Screens.cpp:366','Sources/Core/UI/Screens.cpp:1022'],
 'HD10초와 애니메이션·두 단계 입력을 유지한다. 같은 HD 합성배경을 확대/contain/crop 중 어떤 방식으로 표시할지는 글자·로고 영역을 검토해 결정한다.')
group('save-slots','저장슬롯',['0x3bd24','0x3c41c','0x3de68','0x3ef1c'],['0x2e1a4','0x2e6f8','0x2db48','0x2de4c','0x2e0a0'],
 '양쪽3슬롯과1009선택/1001시작/1004복귀/1010삭제 역할. 일반판 배경960×640JPG, HD768×1024PNG. 선택 미리보기 tick은 일반판에서 난수 이동경로를 만드는 분기가 있고 HD tick은 고정 목표로 easyOut한다.',
 '슬롯 배경 아래 이름·국가·경험치·날짜·크레딧·메크 미리보기가 속한다. 미리보기/그림자 layer를 유지한다.',
 '일반판 하단3버튼80×40이고 중심기준 원점이다. HD도80×40이나 양수화면 다른 위치다. 슬롯선택 tile bounds도 보존한다.',
 '동일 슬롯 재선택/OK에서 시작 또는 이름입력, Delete는확인 후 처리, Back은타이틀. 일반판 네트워크Login 분기는 HD 로컬 시작과 분리한다.',
 ['Sources/Core/UI/Screens.cpp:239','Sources/Core/Session/GameSession.cpp:689','Sources/Platform/Apple/HM2SourceView.mm:111'],
 'HD 로컬슬롯/저장/미리보기tick을 공유한다. phone 미리보기의 random 호출을 이식하지 않는다.')
group('map-loading','맵 이동 가림',['0x3b568','0x3b754'],['0x2da14','0x2d730'],
 '양쪽 StageClearBG와 단계0→1→2→4→5→6, alpha접근/20, .99/.01 경계. 일반판480×320, HD768×1024다.',
 'HD는 화면중심(384,512)의 layer13 이미지. 버튼과독립이며 화면 전체를 가려야 한다.',
 '조작버튼은없다. 패널 clip만 키우고 이미지가 덮지 않으면 가림이 완료되지 않는다.',
 '불투명화→객체비움→타일자원비움→다음맵/게이트설정·게임상태/일시정지조정→투명화→제거 순서.',
 ['Sources/Core/Presentation/MapLoading.cpp:5','Sources/Core/Presentation/MapLoading.cpp:17','Sources/Core/Session/GameSession.cpp:554'],
 '단계·alpha·자원폐기순서를 바꾸지 않고 출력 geometry만 공통 viewport coverage에 연결한다.')
group('system-menu','전장 메뉴',['0x2e480','0x2ca98'],['0x23fc8','0x281e0'],
 '일반판 HUD의GameMenuButtons2×8 atlas에서5행1026..1030을 만든다. HD는DlgSystemMenu4행+닫기이며같은ID1027의역할이일반판임무/HD에어리프트로다르다.',
 '일반판은슬라이딩GameMenuBG자식. HD는중/상/하패널과행버튼·문자자식이다.',
 '일반판행128×32, HD는각타일/원본bound를소비한다. 메뉴가열릴때pause와receiver순서를유지한다.',
 'HD1027에어리프트/1031저장종료/1032외부링크/1029Help/1030닫기. 일반판1026인벤/1027임무/1028에어리프트/1029Help/1030닫기.',
 ['Sources/Core/UI/Screens.cpp:627','Sources/Core/Session/GameSession.cpp:739'],
 'HD기능ID·상태전이를공유한다. 메뉴판본차이를버튼누락으로오판하거나phone숫자만복사하지않는다.')
group('alerts','알림·이름·삭제 팝업',['0x73a8','0x5086c','0x50d2c','0x3de68'],['0x4020','0x5468','0x2de4c','0x2db48'],
 '일반판은QobMessageBox그림/버튼·팽창/축소와문자이벤트경로가있다. HD의GameMain일반알림은UIAlertView이고이름/삭제도OS대화상자경로다.',
 'phone팝업은layer13의게임자식. HD OS팝업은게임자식좌표와분리된호스트경계다.',
 '팝업이열린상태의게임입력차단·중복요청·닫힌뒤pointer취소를검사한다. HUD/상점을UIKit으로이관하는근거가아니다.',
 'HDalertId/버튼인덱스콜백,이름1..10자검사,삭제확인후저장반영을보존한다.',
 ['Sources/Platform/Apple/HM2GameRuntime.mm:43','Sources/Platform/Apple/HM2SourceView.mm:93','Sources/Core/Session/GameSession.cpp:689'],
 'HD기준OS요청의범위만호스트에남긴다. iPhone키보드/안전영역은OS호스트검사로구분한다.')
group('radar','레이더',['0x29dc0','0x29f98'],['0x23c7c','0x23d28'],
 '양쪽RaderMark와월드좌표×mapScale+레이더원점을사용한다. HD는플레이어tile0/BOTtile1/게이트tile2. 일반판확인본문의BOT는tile0이다.',
 'HUD에속하는단일표시원점이다. 월드카메라의보이는범위와레이더축척은같은개념이아니다.',
 '확인한draw에는레이더버튼/직접scissor가없다. 이것을레이더밖그림이항상허용된다는전체규칙으로확대하지않는다.',
 '표시경로이며직접조작콜백은확인된가상입력stub다.',
 ['Sources/Core/UI/Screens.cpp:394','Sources/Core/UI/Screens.cpp:927'],
 'HD마커선택·축척을보존하고레이더앵커만배치한다. 원본폰의BOT타일선택을기기분기로이식하지않는다.')
group('base','기지 화면',['0x3575c','0x34db0','0x354ac'],['0x246d4','0x2555c','0x243b8','0x251d0'],
 '원본기지별table이미지·버튼ID와HD미션알림/설명슬라이드경로가있다. 전체기지좌표표의상세비교는기존package/native감사근거를재사용한다.',
 '기지중심에table좌표를더한다. 선택설명은동적슬라이드·높이팽창이라초기좌표만바꾸면갱신에서복귀한다.',
 'table버튼scale과종류별bound,취소선택해제를따로검사한다.',
 '기지버튼선택후같은버튼Pop에서command/shop/maintenance/slots/게이트를요청한다.',
 ['Sources/Core/UI/Screens.cpp:271','Sources/Core/UI/Screens.cpp:327','Sources/Core/Session/GameSession.cpp:697'],
 '기지별원본데이터를보존한다. UI테이블이있다는이유로코드생성자식과동적갱신을누락하지않는다.','candidate-full-table-and-dynamic-pairing')
group('command','사령부·임무결과',['0x37024','0x3883c','0x39e14','0x37d6c','0x39b7c','0x3a520'],['0x2614c','0x2708c','0x26af8','0x26558','0x269ac','0x279a8','0x25a2c'],
 '양쪽사령부/완수배경·보상아이콘·확인5007경로가있다. HD는상중하조립·리스트·설명애니메이션과완수알림을별도상태로갱신한다.',
 '완수표시는기존리스트를접은뒤같은부모아래보상/확인을그린다. 행수에따른동적위치와텍스트레이어가있다.',
 '원본ID의intData/dataObject와새목록revision을표시와선택에같이연결해야한다.',
 'phone tick 0x39e14는 상태가 바뀌면 상태1에서 0x37d6c 목록 생성, 상태0에서 0x39b7c 완수 확인으로 연결한다. phone 목록 ID5100..5106은 0x3a520이 선택하며 수락5004/결과5007/랭킹5009로 분기한다. HD는 미수락5100..5104와 수락5200..5204를 분리한다. 모든 가변 행의 완전 대응은 확정하지 않았다.',
 ['Sources/Core/UI/Screens.cpp:652','Sources/Core/UI/Screens.cpp:737','Sources/Core/UI/Screens.cpp:778','Sources/Core/Session/GameSession.cpp:722'],
 'HD진행/보상/상태전이를유지하고원본모든가변행의로컬배치를후속검사한다.','candidate-variable-row-pairing')

refs={(v,a) for g in groups for v in ['phone201','hd103'] for a in g['phone' if v=='phone201' else 'hd']}
refs|={('phone201',a) for a in ['0x4bd18','0x4bebc','0x4ca54','0x4cadc','0x4e0e4']}
refs|={('hd103',a) for a in ['0x373bc','0x37958','0x37074','0x37aa8','0x37ccc']}
records={(r['version'],r['address']):r for r in index}
for key in refs:
    if key not in records:records[key]=evidence(*key)
    if records[key]['exports']:records[key]['meaning_status']='selected-contract-reviewed-not-whole-function-equivalence'
source_paths={s.split(':')[0] for g in groups for s in g['current_cpp']}
source_paths|={'Sources/Core/UI/Button.cpp','Sources/Core/UI/Screens.hpp','Sources/Core/UI/Help.cpp','Sources/Platform/Apple/HM2Renderer.mm'}
source_files=[{'path':str(p),'sha256':sha(REPO/p),'bytes':(REPO/p).stat().st_size} for p in sorted(source_paths)]
image_names=set('Main_BG Main_Title Main_TitleBig Main_Episode Main_TapToStart SelSlot_BG SelSlot_Sel SelSlot_Empty InvenBG ShopBG MaintenanceBG AirliftBG MissionList_Mid MissionList_Btm MissionList_Top SysMenuBG StageClearBG ItemDesc TradeItemBG'.split())
images=[]
for v in ['phone201','hd103']:
    inv=json.loads((REPO/f'.local/evidence/universal-package-audit-20261113/{v}-inventory.json').read_text())
    for path,m in inv['members'].items():
        if Path(path).stem in image_names:images.append({'version':v,'path':path,'sha256':m['sha256'],'pixels':m.get('metadata',{}).get('pixels'),'kind':m['kind']})
doc={'schema':'hm2-ui-followup-v1','date':'2026-11-13','head_at_start':'7247972','head_at_collection':subprocess.check_output(['git','rev-parse','HEAD'],cwd=REPO,text=True).strip(),
 'scope':'원본폰2.0.1/HD1.0.3 UI역할·자원·부모·입력·전환과현행HD C++소비연결의정적조사. 전체함수의완전동등성·실행성공을뜻하지않음.',
 'binaries':{'phone201':pm['source_sha256'],'hd103':hm['source_sha256']},
 'counts':{'all_phone_bodies':pm['body_denominator'],'all_hd_bodies':len(hm['methods'])+len(hn['methods']), 'phone_address_range_index':len(phone_ui_addresses),'hd_ui_classes':len(hd_ui_classes),'hd_ui_declared_methods':len(hd_ui_addresses),'contract_groups':len(groups),'confirmed_selected_contract_groups':sum(g['status']=='confirmed-selected-contract' for g in groups),'candidate_exact_pairing_groups':sum(g['status']!='confirmed-selected-contract' for g in groups),'unlocated_contract_groups':0,'referenced_functions':len(refs),'indexed_functions':len(records),'syntax_index_only_functions':sum(r['meaning_status']=='syntax-index-only' for r in records.values()),'additional_instruction_walk_entries':1,'unresolved_dynamic_control_constructors':1},
 'count_limit':'주소 범위/클래스 선언 인덱스는 의미 조사 분모와 다르다. 확정은 행에 쓴 선택 계약에 한정한다. 후보는 모든 분기의 완전 대응을 확정하지 않은 행이며 미구현 수와 같지 않다. GuiCommandCenter0x39e14는 기존1077개 몸체 밖 진입점이므로 별도 명령 추적을 연결했다.',
 'groups':groups,'shared_contracts':[
 {'id':'parent-addition','phone':['0x4bd18','0x4bebc'],'hd':['0x373bc','0x37958'],'fact':'부모위치를자식위치에더한다. 부모의scale/rotation을자식offset에일반affine변환처럼자동곱하지않는다. 카메라·fixedPos스냅은별도소비단계다.'},
 {'id':'direct-scissor-scan','fact':'전체1077+1300본문을직접glScissor/명시clip selector로검색했다. 일치한직접호출은phone EAGL0x3ce8와HD EAGL0x3514뿐이다. UI별clip가없다는전체의미증명은아니다. 가림이미지·부모 show·modal입력차단은scissor와구분한다.','matches':clip},
 {'id':'button-edge-and-order','phone':['0x4ca54','0x4cadc'],'hd':['0x37aa8','0x37ccc'],'fact':'원본별bound저장·타일교체·scale설정순서와경계포함규칙이다르다. 현행Button은HD의좌/하제외·우/상포함을사용하고역등록순서로input을배달한다. 시각scale과터치영역을같다고추정하지않는다.'},
 {'id':'coverage-registry-consistency','source':'Sources/Core/UI/Screens.cpp:1109','fact':'coverage()의일부주소가현행원본캐시와다르다(WndInven0x34918대실제init0x3490c,DlgSystemMenu0x23fb0대실제init0x23fc8). shop item-detail에false가남지만buildDetail/layoutDetail/tickDetail코드연결은존재한다. 실행완료나전체기능완성을의미하지않는다.','todo':'등록표주소·상태를원본근거및실제검사와맞추고,단순coverage숫자로완료/미구현을판정하지않는다.'}],
 'development_boundaries':[
 '한벌HD이미지·공통C++20게임/UI코어를유지한다. iPhone전용이미지/원본폰에셋/별도게임코어파일을추가하지않는다.',
 '공통DisplayMetrics/Viewport/Layout에서안전영역·방향·GPU영역·패널anchor를계산한다. Screens생성/갱신,Button hit,Session drag,Help,MapLoading,Transition이같은결과를소비한다.',
 '원본HD의위치더하기·fixedPos·layer·시각배율·bound설정순서를보존한다. Layout 변환은조립이끝난화면geometry와역입력에적용하고무기/이동/충돌/아이템인덱스에는적용하지않는다.',
 '합성이미지는같은GPU텍스처의UV/clip/contain/확대이동/스크롤로표시할수있다. 글자있는배경을임의비율왜곡하거나잘라새파일로저장하지않는다. 레이아웃확장은원본행동으로오인하지않게기록한다.',
 '폰/HD의수량·ID·시간차이는HD게임규칙을공유한다. 폰의20칸/16상품/5공수/11초/미리보기RNG를기기분기로복제하지않는다.',
 'HD phase/tick·원본시간·저장·음향/난수호출순서와coverage geometry를분리한다. 전환과로딩의상태판정상수를화면높이로일괄치환하지않는다.',
 '호스트UIKit은기존HD OS대화상자/이름입력·키보드/방향·안전영역연결에한정한다. 인게임HUD/상점/패널을UIKit으로새로만들지않는다.'
 ],
 'required_checks':[
 'iPad세로HD기준좌표·타일·상태·입력·layer·동시치트장면을보존하고iPhone가로viewport의모든실제Session에같은layout을주입한다.',
 '모든화면/팝업에서표시bbox·필수텍스트·버튼hit·드롭격자·모달차단영역을수집한다. 중첩·안전영역침범·음수크기·잘림·빈영역입력누출을검사한다.',
 '인벤25칸/장비6/BOT3/퀵5/상점24의첫끝칸·페이지·비어있음·최대갯수·재고0·해금/미해금조건을동시에검사한다.',
 '클릭/이동/밖으로release/cancel/동일ID멀티터치/화면전환중cancel/팝업동적등록을검사한다. move와end에서소비여부가달라도capture를보존한다.',
 '각창생성직후와후속tick에서같은배치를검사한다. 아이템상세·기지설명·타이틀·무기선택·사령부행의원위치복귀누락을검출한다.',
 'HD폰트요청(현재이름/크기/정렬/텍스트영역)을보존하고폰가로실효표시크기·한국어줄바꿈·본문잘림을기록한다. 원본미측정폰트최소크기를임의합격기준으로만들지않는다.',
 'TradeItemBG의전체modal hit와StageClearBG/전환패널의전체불투명coverage를자원픽셀크기와별도로검사한다.',
 '공통화면구성에따라HUD가월드선택·사격을가리는영역과safe-area입력역변환을검사한다. Camera/isInScreen/AI/spawn/RNG검사는네이티브공통계획에서별도유지한다.',
 '소스연결검사와실행검사를분리한다. 동시치트및정식플레이검사후마지막에HD/iPad포팅판과일괄대조한다. 구현중기능별포팅판왕복·iPhone에서HD로직교차검증은금지한다.'
 ],
 'open_items':[
 {'id':'layout-fit-policy','state':'unmeasured-runtime','detail':'HD합성이미지/한국어본문의폰가로가독성,contain/확대이동/스크롤정책과터치가능영역은실제표시측정전이다.'},
 {'id':'variable-row-and-detail-pairing','state':'candidate','detail':'기지표/사령부가변행/상세팽창의모든폰↔HD분기대응은위3개후보행에남겼다. HD기준소비연결이없다는뜻은아니다.'},
 {'id':'button7000','state':'unresolved-constructor','detail':'HD GuiGame.handleButton의7000분기는기존coverage에생성자미확정이다. 정적native담당에게실제생성/도달조건을요청했다. 임의버튼을추가하지않는다.'},
 {'id':'coverage-registry','state':'todo','detail':'주소/상태등록표를실제근거및검사에맞게보강한다. 이조사는게임소스를변경하지않았다.'}
 ],
 'collector_program':{'path':'tools/audit_iphone_ui_followup.py','sha256':sha(Path(__file__))},'source_files':source_files,'image_metadata':images,
 'evidence_inputs':[{'path':str(p),'sha256':sha(p)} for p in [PHONE/'export-manifest.json',HD/'method-evidence/export-manifest.json',HD/'native-evidence/export-manifest.json',HD/'objc-layouts.json',cfs_path,REPO/'.local/evidence/universal-native-audit-20261113/phone-rtti-vtables.json']],
 'functions':list(sorted(records.values(),key=lambda r:(r['version'],int(r['address'],16)))),
 'not_performed':['게임소스변경없음','앱실행/빌드/시뮬레이터/기기조작없음','REA session조작/라이브쿼리없음','원본전체코드/표/그림복사없음']}
target=REPO/'docs/evidence/iphone-universal-ui-followup-20261113.json'
target.write_text(json.dumps(doc,ensure_ascii=False,indent=2)+'\n')
(OUT/'collection-summary.json').write_text(json.dumps({'counts':doc['counts'],'document':str(target),'sha256':sha(target)},ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'counts':doc['counts'],'document':str(target),'sha256':sha(target)},ensure_ascii=False))
