# iPhone 상점 원본 좌표와 실기기 설치

상점과 Equip의 원본 그림에는 빈 슬롯과 하단 탭이 그려져 있다. 이전 복원판은 화면 폭에 맞춰 그림의 일부를 별도로 확대하고, 슬롯과 버튼을 다른 변환으로 배치했다. 그래서 아이콘 크기를 맞춘 뒤에도 행·열 간격이 달랐고 SG-1 상세창이 원본보다 약 25pt 위에 있었다.

iPhone 상점·Equip 그림, 버튼, 터치, 드롭 좌표에 하나의 480×320 변환을 적용했다. 중앙의 메뉴 그림은 원본 비율과 네 행을 유지한다. 현대 iPhone에서 더 넓어진 좌우에는 해당 기지의 원본 그림을 한 번 그린다. 메뉴 그림을 늘리거나 복제할 필요가 없다. 버튼과 장비 배치는 공통 C++에 있다.

- 상품 4열×4행과 인벤토리 5열×4행의 중심 간격은 원본과 같은 가로·세로 40단위다. iPhone Air에서는 52.5pt다. 게임 데이터의 24상품·25인벤토리 인덱스는 유지하고 다음 화살표로 넘긴다.
- Ruin Field Base 레벨 1, 500 Cr, SG-1 상세를 원본과 복원판에서 비교했다. 912×420으로 방향과 해상도를 맞춘 PNG에서 상세창 가로 외곽선의 세로 차이는 최대 2픽셀이다. 480×320과 667×375에서도 상세창 터치 영역과 BUY가 겹치지 않는지 검사했다.
- Equip에서 상점으로 바뀔 때 새 텍스처 준비 목록에 기지 그림을 다시 등록한다. 직접 정비 진입 뒤 상점으로 넘어가는 경로, 인벤토리 페이지와 드롭, 마지막 페이지 유지, SG-1 구매·판매·취소와 ML-K3를 확인했다.
- iPad Air 시뮬레이터에서 여섯 기지 상점의 픽셀 차이는 0개였다. 실기기 iPad에는 서명된 Release를 설치했고 설치 전후 저장 28,204바이트가 일치했다. 기기 잠금 때문에 앱 실행이 거부됐다. 실기기 iPhone은 페어링돼 있으나 Xcode 연결이 불가해 설치하지 못했다.

구현 커밋은 `5759f2b`이다. 공통 표시·입력 검사와 iPhone Native 5개, iPad Native 1개를 통과했다. 치트빌드는 두 시뮬레이터에서 각각 126조건·2,100프레임을 실행했고 버튼 실패는 0건이었다. iPhone에서도 기지 여섯 곳의 상점 PNG를 확보했다.

## 캡처와 검사

원본은 `.local/evidence/iphone-shop-20261119.7atGic/original/07-weapon-detail.png`에 있다. 복원판 레벨 1은 `.local/evidence/iphone-shop-reference-layout-20261119.0sCST9/iphone-reference-final/live-phone-reference-level1-sg1-detail.png`에 있다. 같은 폴더의 `sg1-detail-pixel-rows.json`에 외곽선 비교를 기록했다. [파일과 실행 결과](evidence/iphone-shop-reference-coordinates-20261119.json)

## 원본 와이드 확인

일반판 2.0.1의 `EAGLView`는 기기가 준 화면 크기로 투영과 뷰포트를 설정한다. 상점 쪽 `0x403b0`은 화면 상태에 따라 `ShopBG.jpg` 또는 `MaintenanceBG.jpg`를 고르고, `0x4d4b8`이 타일과 프레임을 지정한다.

원본 IPA와 바이트가 일치하는 두 960×640 JPEG를 각각 열어 확인했다. `ShopBG.jpg`에는 패널 테두리, 빈 슬롯, `ITEM SHOP`과 `Inventory` 제목, 하단 `Shop`·`Equip` 탭 그림이 들어 있다. `MaintenanceBG.jpg`에도 패널과 빈 슬롯, 정비 화면의 고정 제목과 탭 그림이 들어 있다. 상품 상세창과 거래 확인창은 각각 `ItemDesc.png`와 `TradeItemBG.png`라는 별도 리소스다. 현재 포팅판은 상품 이름·설명·가격을 데이터에서 읽어 그린다. 구매·판매·나가기·정렬·페이지 버튼도 개별 리소스를 쓴다.

기본 포팅판은 호환 런타임에서 480×320을 화면 중앙에 비례 배치했다. 복제한 포팅판의 설정에 `--logical-size=320x695`를 넣고 Wasteland Shop까지 들어갔다. 실제 가로 출력 2,736px에서 상점 그림이 왼쪽 약 1,308px에만 나타났다. 원본 포팅판의 저장 파일은 검사 전과 바이트가 일치했다.

`5759f2b`에서 추가한 허브 그림은 배경 결함으로 추적한다. Apple은 게임 메뉴가 다양한 화면 비율에 맞게 배치되고, 안전 영역 안에서 조작이 가능해야 한다고 안내한다. 배경은 화면 전체를 사용한다. [Apple 게임 디자인 지침](https://developer.apple.com/design/human-interface-guidelines/designing-for-games/) · [Apple WWDC 안전 영역과 전체 화면](https://developer.apple.com/videos/play/wwdc2024/10085/?time=549)

와이드 실측과 PNG는 `.local/evidence/iphone-shop-viewport-audit-20261119.TpRaGA/viewport-observations.json`과 같은 폴더의 `wide-wasteland/wide-wasteland-shop.png`에 있다.

## iPhone 상점 배경과 패널 분리

`ShopBG.jpg`의 칸, 제목, 패널, 탭을 제거해 격납고만 남겼다. 원본 Phone과 같은 격납고가 그려진 HD 그림을 참고해 가려진 부분을 ImageGen으로 보정했다. 가로 화면에서 천장과 바닥이 들어오도록 양옆을 확장했다. 원본에서 보이는 구도와 조명 위치를 기준으로 결과를 비교했다. 가려진 부분과 양옆은 보정한 픽셀이다.

`PhoneShopHangar.jpg`는 1902×827이며 화면 중앙에서 가로·세로에 같은 배율을 적용한다. 두 960×640 투명 패널의 칸·제목·탭은 원본 JPEG에서 분리했다. Shop과 Equip는 같은 격납고 배경을 쓰고 패널만 교체한다. `Screens::Impl::phoneShop`에서 기지 그림을 읽던 경로를 제거했다. 상품·상세창·거래 버튼은 기존 좌표와 입력을 쓴다. 파일은 `Resources/PhoneShopArt/`에 두고 준비 단계에서 원본과 구분해 해시를 기록한다.

공통 검사 5개와 iPhone 시뮬레이터 검사 4개가 통과했다. 웨이스트랜드 다섯 분류와 Equip 전환, SG-1 상세, ML-K3 구매 확인창을 캡처했다. iPhone의 기지 6곳 상점과 Equip 4가지 크기에서 가장자리의 순수 검은 픽셀과 동일한 끝 픽셀 열은 각각 0건이었다. iPad 시뮬레이터의 기지 6곳 상점은 직전 후보와 달라진 픽셀이 0개였다. 양쪽 메뉴 36개에서 번들 배경 파일의 해시를 대조했다. 실기기 확인은 투두에 남겼다.

캡처와 로그는 `.local/evidence/iphone-shop-clean-art-20261119.Dqz7aA/`에 있다. 별첨은 `iphone-wasteland-shop.png`, `iphone-wasteland-equip.png`, `iphone-sg1-detail.png`, `ipad-wasteland-shop.png`다. 측정 결과는 `iphone-edge-pixels.json`과 `ipad-shop-pixel-match.json`에 있다.

## 수정한 배경을 적용한 상점의 페이지 표시

일반판 2.0.1의 웨이스트랜드 레벨 22에서 Weapon과 Parts는 여러 화면으로 넘어가며 좌우 화살표가 보였다. BOT·Skill·Etc는 한 화면에 들어와 화살표가 없었다. 기존 복원판은 한 화면인 목록에도 화살표를 표시했고, HD의 같은 페이지 안에서 다음 묶음으로 넘어가면 점 표시가 움직이지 않았다.

`Screens`에서 상점에 실제로 보이는 묶음이 둘 이상일 때 페이지 화살표를 등록한다. 상품·인벤토리의 점은 화면에 보이는 묶음 번호를 따른다. HD 페이지당 24상품·25인벤토리와 정렬·드롭은 기존 코드를 사용한다. iPhone 상품 상세 설명은 원본 캡처와 같은 흰색을 쓴다. 보고서의 격납고 JPEG를 Release 번들에서도 바이트 단위로 확인했다.

공통 코어 검사 5개와 iPhone GPU 검사 5개를 통과했다. 공통 세션에서 SG-1 구매와 판매를 검사했다. iPhone GPU에서 웨이스트랜드 다섯 분류, ML-K3 상세와 구매 확인, 세 번째 BOT의 DISARM과 능력치 세 줄을 캡처했다. iPhone 상점 6곳과 Equip 4가지 크기의 캡처를 확인했다. iPad 상점 6곳의 이전 캡처와 달라진 픽셀은 0개였다.

정식 Release 앱은 두 시뮬레이터에 설치해 시작을 확인했다. 앱 호스트를 참조하는 XCTest 링크가 Release에서 실패해 GPU와 입력 검사는 Debug에서 실행했다. 실기기 iPhone은 연결 불가였다. 캡처와 두 구성의 로그는 `.local/evidence/iphone-shop-ui-release-20261119.VKfBFH/`에 있다.
