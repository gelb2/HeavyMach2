# 일반판·HD판 분리의 기술적 배경

2026-11-10. 확인된 패키지 차이와 당시 플랫폼 환경에서 가능한 이유를 구분한다. 원작자의 분리 결정에 관한 인터뷰·설계 문서·원본 소스는 확보하지 않았다.

## 확인된 사실

- Apple은 2010-01-27 iPad 발표 당시부터 iPhone·iPad·iPod touch에서 실행할 유니버설 앱 제작을 지원한다고 설명했다. 따라서 당시 기술이 두 앱 출시를 강제했다고 설명할 수 없다. [Apple 발표](https://www.apple.com/ie/newsroom/2010/01/27Apple-Launches-iPad/), [당시 SDK 안내](https://developer.apple.com/news/?id=01272010a).
- 확보한 일반판 2.0.1의 bundle ID는 `kr.indieapps.HeavyMach2`, 기기 family는 1이다. HD 1.0.3은 `kr.indieapps.HvM2HD`, family는 2다. 별도 앱 제품임은 패키지에서 확인했다.
- 일반판 실행파일에는 ARMv6·ARMv7 slice가 있고 HD에는 ARMv7 하나가 있다. 일반판은 더 오래된 CPU를 포함하는 빌드 구성이며, HD는 다른 지원 범위로 빌드됐다. 정확한 최소 OS와 모든 지원 모델은 이 사실만으로 확정하지 않는다.
- 포팅판에서 일반판 게임 화면은 480×320 가로, HD는 768×1024 세로로 관측했다. 화면 총 픽셀 수는 5.12배이고 방향·종횡비도 다르다. HD HUD는 별도 배치다. 출시 당시 모든 판본·기기의 방향 지원까지 확인한 것은 아니다.
- 비교한 동명 PNG 중 839개는 바이트 동일, 357개는 다르고 그중 342개는 크기도 다르다. 3개 MCB는 모두 동일하며, 135개 TBLE 중 59개는 다르다. 일반판만의 PNG 40개와 HD만의 PNG 42개도 있어 HD 자원을 무조건 전부 대응된다고 간주하지 않는다. [패키지 비교 근거](evidence/standard-hd-package-metadata.json).
- 실행파일 코드 영역이 다르다는 것은 확인했으나, 두 판본은 버전·CPU·빌드 구성이 다르다. 이를 근거로 원작자가 소스를 두 벌로 완전히 분리해 작성했다고 결론 내리지 않는다. 같은 소스의 타깃·조건부 컴파일·라이브러리 차이로도 실행파일은 달라질 수 있다. 정확한 공유 코드 범위는 함수 단위 대조가 필요하다.

## 분리 개발·출시를 선택할 수 있었던 기술적 이유

1. **해상도와 화면 구성:** 작은 화면의 그림을 확대하면 새 디테일이 생기지 않는다. 큰 화면에서는 고해상도 그림과 다른 HUD·메뉴 배치가 필요했다. 화면 좌표에 직접 결합된 UI·카메라 코드가 있었다면 iPad 타깃에서 별도로 조정하는 방식이 간단했을 수 있다. HM2 내부의 결합 정도와 개발자의 실제 선택 이유는 미확인이다.
2. **구형 기기의 자원 비용:** 큰 그림을 작게 그려도 원본 텍스처를 그대로 로드하면 텍스처 메모리 비용은 줄지 않는다. 같은 비압축 픽셀 형식에서 폭·높이를 각각 2배로 만들면 픽셀 수와 해당 텍스처 데이터량은 4배다. 저해상도 자원만 가진 일반판 패키지는 구형 기기의 다운로드·저장 공간과 메모리 부담을 줄일 수 있다. 이는 가능한 동기이며 HM2의 당시 병목을 측정한 결과가 아니다. Apple도 텍스처 크기·메모리·대역폭과 고해상도 렌더링 비용을 설명한다. [텍스처 안내](https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/OpenGLES_ProgrammingGuide/TechniquesForWorkingWithTextureData/TechniquesForWorkingWithTextureData.html), [고해상도 화면 안내](https://developer.apple.com/library/archive/documentation/2DDrawing/Conceptual/DrawingPrintingiOS/SupportingHiResScreensInViews/SupportingHiResScreensInViews.html).
3. **빌드·회귀 검사 범위:** 지원 CPU·OS·자원·UI를 타깃별로 고정하면 기존 일반판에 iPad 대응 변경이 미치는 영향을 좁힐 수 있다. 별도 앱은 출시·수정 일정을 독립적으로 운영할 수도 있다. HM2가 실제 이 이유로 분리됐다는 직접 증거는 없다.

각 이유는 한 앱 안에서도 기기별 자원 로딩·레이아웃·지원 설정으로 해결할 수 있다. Apple의 번들 문서도 유니버설 앱의 iPhone/iPad별 자원 선택을 설명한다. 다만 HM2의 자체 PNG·MCB reader가 UIKit의 자원 선택을 자동으로 사용했다고 가정하지 않는다. [기기별 자원 선택](https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFBundles/AccessingaBundlesContents/AccessingaBundlesContents.html).

별도 HD 가격·재구매·판매 전략도 앱 분리의 가능한 사업적 요인이지만, 이 프로젝트의 실제 가격 이력과 개발자 설명을 확보하지 않았으므로 분리 원인으로 단정하지 않는다.

## 이번 복원에 적용할 결론

두 앱이라는 배포 형태를 재현할 필요는 없다. 원작의 그림·비례·조립·게임 규칙·기기별 표시를 보존하면서 **HD 그래픽 공통 사용 + 검증된 공통 코어 + 기기별 표시 구성**으로 유니버설 앱을 만든다. HD 그래픽 사용 시의 실제 텍스처 메모리·로딩·프레임 비용은 후속 앱에서 측정한다. 원작 비교용 일반판·HD 포팅 앱은 계속 별도로 보존한다. 이 문서는 방향과 배경 기록이며 구현 완료 보고가 아니다.
