# SR110 RDK — SSD1306 OLED 디스플레이 연결 실험

**대상**: SR110 RDK (Astra Machina Micro), Zephyr (`sr100_rdk/sr100/m55`)
**목적**: SR110에 별도 디스플레이 인터페이스가 없는 상황에서, I2C를 통해 외부 소형 OLED를 연결·구동하는 방법 정리

---

## 1. SSD1306이란

**SSD1306**은 Solomon Systech이 만든 **모노크롬 OLED 디스플레이 컨트롤러 IC**입니다. 흔히 "SSD1306 디스플레이"라고 부르지만, 정확히는 **디스플레이 패널이 아니라 그 패널을 구동하는 컨트롤러 칩**이며, 소형 OLED 패널(보통 0.91"~1.3", 128×32 또는 128×64 해상도)에 이 컨트롤러가 함께 실장되어 하나의 모듈로 판매됩니다.

### 주요 특징

| 항목 | 내용 |
|---|---|
| 디스플레이 타입 | 모노크롬(흑백 1비트) OLED |
| 해상도 | 보통 128×64 (128×32 버전도 있음) |
| 통신 인터페이스 | **I2C** 또는 **SPI** (모듈마다 배선/저항 설정으로 선택) |
| 동작 전압 | 통상 1.65V~3.3V (VDD), 모듈 자체 승압 회로가 OLED 구동 전압을 만듦 |
| 메모리 구조 | 내장 GDDRAM(Graphic Display Data RAM) — 컨트롤러가 프레임버퍼를 자체 보관, MCU는 필요할 때만 갱신 데이터를 씀 |
| 어드레싱 방식 | "Page" 단위 — 세로 8픽셀을 1바이트로 묶어서 씀 (아래 4-2절 참고) |
| 대표 I2C 주소 | `0x3C` (일부 모듈은 `0x3D`) |

### 왜 임베디드 실습에 많이 쓰이나

- 배선이 단순함 (I2C 모드면 SCL/SDA/VCC/GND 4선)
- 소비 전력이 매우 작음 — 배터리 구동 IoT 기기와 궁합이 좋음 (SR110의 저전력 특성과 잘 맞음)
- Zephyr를 포함한 대부분의 임베디드 프레임워크에 **표준 드라이버가 이미 있어서**, 레지스터 레벨 코드를 직접 짤 필요가 없음

---

## 2. 왜 이 실험이 필요했나 — SR110에는 디스플레이 인터페이스가 없음

SR110 제품 브리프와 실제 회로도(`SC950-C01116-01`)를 확인한 결과, SR110에는 **MIPI-DSI, parallel RGB, LVDS 같은 디스플레이 전용 출력 인터페이스가 전혀 없습니다.** 있는 건:

- MIPI-CSI RX ×2 + DVP — **카메라 입력 전용**
- MIPI-CSI TX ×1 — 카메라 영상을 다른 호스트로 전달하는 패스스루용이지 디스플레이 구동용이 아님

이는 우연이 아니라 제품 포지셔닝입니다 — SR110은 "보고 듣고 판단"하는 초저전력 센싱 MCU이고, 디스플레이가 필요한 제품은 GPU/디스플레이 파이프라인을 갖춘 상위 SL시리즈(Linux 구동)가 담당하는 구조입니다.

**따라서 SR110에서 디스플레이를 쓰려면, 전용 인터페이스가 아니라 이미 있는 범용 버스(I2C/SPI)로 자체 컨트롤러 내장 소형 디스플레이를 "소프트웨어로" 구동해야 합니다.** SSD1306이 이 용도에 가장 적합한 이유는 위 1절의 특징(단순 배선, 저전력, 표준 드라이버 존재) 때문입니다.

---

## 3. 인터페이스 선택 — I2C0 vs I2C1

SR110 RDK(J25, 20핀 헤더)에는 I2C 버스가 물리적으로 2개 노출되어 있습니다.

| | I2C0 (J25 pin 3/4) | I2C1 (J25 pin 15/16, 회로도 표기 "SWIRE_CLK/DATA") |
|---|---|---|
| 실제 GPIO | GPIO15/GPIO16 | GPIO42/GPIO43 |
| devicetree 상태 (기본) | **비활성 — `&i2c0` 노드 자체가 없음** | 활성 (`&i2c1`) |
| 이미 물려있는 것 | 없음 | IMU(MC3479, 0x4C) · ALS(TCS34303, 0x39) · GPIO 익스팬더(0x20) · RTC(BU9873, 0x32) |

**이번 실험은 I2C0을 선택했습니다.** SSD1306의 I2C 주소(0x3C/0x3D)가 기존 4개 디바이스 주소와 겹치지는 않아서 I2C1을 같이 써도 기술적으로는 문제없지만, **완전히 비어있는 버스를 새로 쓰는 쪽이 다른 디바이스와의 버스 경합·타이밍 간섭 가능성을 원천적으로 없앨 수 있어** 실습용으로 더 깔끔합니다.

대신 이 선택 때문에, 기존에 한 번도 활성화된 적 없는 **`&i2c0`를 devicetree 오버레이로 새로 켜야 하는 작업**이 추가로 필요했습니다 (5-1절 참고).

---

## 4. 하드웨어 연결

### 4-1. J25 핀맵 (실측 확인됨)

| Pin | 신호 | Pin | 신호 |
|---|---|---|---|
| 1 | 1.8V | 2 | GND |
| **3** | **I2C0_MS_SCL** | **4** | **I2C0_MS_SDA** |
| 5 | CIU_VSYNC | 6 | GPIO5 |
| 7 | GPIO6 | 8 | GPIO8 |
| 9 | GPIO9 | 10 | GPIO7 |
| 11 | SPI_MSTR_CLK | 12 | SPI_MSTR_CS |
| 13 | SPI_MSTR_MISO | 14 | SPI_MSTR_MOSI |
| 15 | SWIRE_CLK (I2C1_SCL) | 16 | SWIRE_DATA (I2C1_SDA) |
| 17 | CIU_D6 | 18 | CIU_D7 |
| 19 | 1.8V | 20 | GND |

### 4-2. 배선표

```
SSD1306 모듈    J25
VCC       →    Pin 1 또는 19 (1.8V)
GND       →    Pin 2 또는 20
SCL       →    Pin 3  (I2C0_MS_SCL)
SDA       →    Pin 4  (I2C0_MS_SDA)
```

### 4-3. ⚠️ 로직 레벨 주의 (가장 중요한 하드웨어 위험 요소)

SR110의 GPIO는 **1.8V 로직 전용**입니다(제품 브리프: "Up to 43 general-purpose **1.8V** inputs/outputs"). 그런데 시중에 흔한 저가 SSD1306 브레이크아웃 모듈 대부분은 **자체 풀업 저항을 모듈 자신의 VCC(보통 3.3V)에 물려서** 나옵니다.

이 상태로 그냥 연결하면 I2C 버스 전체가 3.3V로 끌려 올라가면서 **SR110 GPIO에 정격 이상의 전압이 걸릴 위험**이 있습니다. 아래 중 하나로 반드시 해결해야 합니다.

1. **가장 안전** — 모듈 VCC를 1.8V 레일에서 공급 (SSD1306 자체는 통상 1.65~3.3V 범위에서 동작하지만, 모듈에 따라 실제 지원 여부가 다르므로 데이터시트 확인 필요)
2. 모듈의 자체 풀업 저항을 물리적으로 제거하고, SoC 내부 풀업만 사용
3. 양방향 I2C 레벨 시프터(예: TXS0102)를 모듈과 보드 사이에 추가

---

## 5. 소프트웨어 구조

### 5-1. Devicetree — I2C0 신규 활성화 + SSD1306 노드

`sr100_rdk_m55.dts`/`m4.dts` 어디에도 `&i2c0`가 없어서, 오버레이로 새로 켜야 합니다.

```dts
/* app.overlay */
&i2c0 {
	status = "okay";
	pinctrl-names = "default";
	pinctrl-0 = <&i2c0_ms_scl &i2c0_ms_sda>;
	clock-frequency = <I2C_BITRATE_STANDARD>;

	ssd1306: ssd1306@3c {
		compatible = "solomon,ssd1306";   /* "fb" 없음 -- 아래 참고 */
		reg = <0x3c>;
		width = <128>;
		height = <64>;
		segment-offset = <0>;
		page-offset = <0>;
		display-offset = <0>;
		multiplex-ratio = <63>;
		segment-remap;
		com-invdir;
		prechargep = <0x22>;
	};
};
```

> **pinctrl 노드명 확인 완료 (실사용자 확인, 2026-08)**: `sr100_pinctrl.dtsi`를 직접 grep한 결과 실제 노드명은 **`i2c0_ms_scl`**, **`i2c0_ms_sda`**로 확인됐습니다 — `&i2c1`이 "_b" 접미사(`i2c1_ms_scl_b`)를 쓰는 것과 달리, **I2C0은 접미사가 붙지 않습니다.**
>
> **`compatible` 문자열 정정 (가장 결정적인 발견, 실사용자 확인됨)**: 처음엔 `"solomon,ssd1306fb"`("fb" 있음)로 썼는데, 이게 틀렸습니다. **Zephyr v4.4.1의 정확한 값은 `"solomon,ssd1306"`("fb" 없음)** 입니다. 이 문자열이 틀리면 `zephyr\drivers\display\Kconfig.ssd1306`의 `depends on DT_HAS_SOLOMON_SSD1306_ENABLED` 조건이 거짓이 되어, **`CONFIG_SSD1306`이 `.config`에서 "꺼짐"이 아니라 아예 항목 자체가 안 보이게 됩니다** — 그 상태로 빌드하면 링크 단계에서 `undefined reference to __device_dts_ord_N` 에러가 납니다. 처음엔 "SDK에 display 드라이버가 아예 없나"로 오해했지만, **드라이버는 처음부터 있었고 devicetree 문자열 하나가 틀렸던 것**으로 최종 확인됐습니다 (4-4절 참고).

### 5-2. 디버깅 사례 — "드라이버가 없다"는 오해와 진짜 원인

이 실험에서 가장 시간이 걸렸던 부분이라 기록해둡니다.

**증상**: devicetree(`zephyr.dts`)엔 `ssd1306` 노드가 `status = "okay"`로 정확히 들어가 있는데, 빌드 마지막에 링커 에러가 남:
```
undefined reference to `__device_dts_ord_95'
```
그리고 `build\zephyr\.config`를 열어봐도 `CONFIG_SSD1306`이 "꺼짐(`is not set`)"조차 아니고 **항목 자체가 없음**.

**처음 세운(틀린) 가설**: "Synaptics SDK에 포함된 Zephyr에는 애초에 display 드라이버가 없는 것 아닌가?" → 이 가설을 따라갔다면 SDK의 `zephyr/` 전체를 순정 zephyrproject 코드로 교체하는, **훨씬 위험하고 불필요한 작업**으로 이어질 뻔했습니다 (SR100 전용 패치가 깨질 위험).

**진짜 원인**: `zephyr\drivers\display\Kconfig.ssd1306`을 직접 열어보니 드라이버는 처음부터 있었고, 다음 조건으로 활성화됩니다:
```
depends on DT_HAS_SOLOMON_SSD1306_ENABLED || DT_HAS_SOLOMON_SSD1309_ENABLED || DT_HAS_SINOWEALTH_SH1106_ENABLED
```
Zephyr는 devicetree의 `compatible = "vendor,part";` 문자열로부터 `DT_HAS_VENDOR_PART_ENABLED`라는 Kconfig 심볼을 자동 생성합니다. 오버레이에 썼던 `"solomon,ssd1306fb"`("fb" 있음)는 `DT_HAS_SOLOMON_SSD1306FB_ENABLED`를 만드는데, 정작 Kconfig가 기다리는 건 `DT_HAS_SOLOMON_SSD1306_ENABLED`("fb" 없음) — **이름이 안 맞아서 조건이 계속 거짓**이었던 겁니다. 그래서:
- Kconfig 메뉴에서 `SSD1306` 옵션 자체가 숨겨짐 (꺼진 게 아니라 "존재 자체가 안 보임")
- 드라이버 소스가 컴파일 대상에서 빠짐 → 그 드라이버가 정의해야 할 디바이스 구조체(`__device_dts_ord_95`)가 안 만들어짐 → 그걸 참조하는 `main.c`가 링크 단계에서 실패

**교훈**: Zephyr는 코드 안에서 `compatible` 문자열을 **버전마다 정리(rename)** 하는 경우가 있습니다 — 이번 Zephyr v4.4.1에서는 `"solomon,ssd1306"`이 맞는 값이었습니다. **"드라이버가 없다"는 결론을 내리기 전에, 항상 `Kconfig.<드라이버명>` 파일을 먼저 열어서 실제 `depends on DT_HAS_..._ENABLED` 조건과 devicetree 바인딩(`.yaml`)의 `compatible:` 값을 대조하는 것이 먼저**입니다 — 이번처럼 `.config`에 옵션 자체가 안 보이는 증상은 대부분 "드라이버 부재"가 아니라 "devicetree 문자열 불일치"일 가능성이 훨씬 높습니다.

### 5-3. Zephyr Display 서브시스템 — 왜 레지스터를 직접 안 건드리나

Zephyr는 SSD1306용 **공식 표준 드라이버(`solomon,ssd1306fb`)** 를 이미 갖고 있습니다. 이 드라이버가 초기화 시퀀스(전원 관리, 컨트라스트, 스캔 방향 등 SSD1306 커맨드셋)를 전부 대신 처리해주기 때문에, 애플리케이션 코드는 표준 **Display API**(`display_write()`, `display_blanking_off()` 등)만 호출하면 됩니다 — 이 프로젝트의 다른 예제들(GPIO/I2C 등)이 SR100 전용 레지스터를 직접 다뤘던 것과 달리, 이번엔 **범용 프레임워크 API 그대로**라 확신도가 가장 높은 부류의 실험입니다.

### 5-4. SSD1306 어드레싱 방식 — Page 단위

SSD1306은 화면을 가로 128픽셀 × 세로 8개 "페이지"(각 페이지=세로 8픽셀)로 나눠서 관리합니다. 즉 **프레임버퍼 1바이트 = 화면의 세로 8픽셀 한 줄**입니다. 실험 코드에서 세로 막대를 그릴 때 각 페이지마다 해당 컬럼 바이트를 `0xFF`로 채우는 것도 이 방식 때문입니다.

---

## 6. 실험 코드 설계 — 3단계 검증

단순히 "화면 전체를 흰색으로 채우고 끝"내지 않고, **3단계로 나눠서 각 단계가 서로 다른 것을 증명**하도록 설계했습니다.

| 단계 | 동작 | 이게 증명하는 것 |
|---|---|---|
| 1 | 화면 전체 흰색 (2초) | 패널·배선·I2C 통신이 최소한 살아있다 |
| 2 | 화면 전체 검정 (2초) | 단순히 "항상 켜진 상태"가 아니라, MCU가 실제로 다른 데이터를 내려보낼 수 있다 |
| 3 | 세로 막대가 좌우로 이동 | `display_write()`의 좌표(`x`,`y`)·`pitch` 계산이 정확하다 — 우연히 풀스크린만 되는 게 아니라 **부분 갱신·애니메이션까지 정상 동작**한다 |

1~2단계만으로는 "혹시 항상 같은 패턴만 뜨는 하드웨어 결함을 정상으로 착각"할 위험이 있어서, 3단계(움직이는 막대)를 반드시 포함해 좌표 계산 자체가 맞는지까지 확인하게 했습니다.

---

## 7. 빌드 / 플래시 / 실행 확인

**실행 위치**: PowerShell, 워크스페이스 루트

```powershell
west build -p always -b sr100_rdk/sr100/m55 <ssd1306_i2c0_lab 경로>
```

플래시는 지금까지와 동일 (`srsdk_tools`의 `srsdk_image_generator.py` + `openocd_flash.py`).

**실행 확인** (J14, 115200bps)

기대 로그:
```
SSD1306 I2C0 lab starting
Display ready: 128x64
Step 1: full white screen -- check the panel is lit solid white
Step 2: full black screen -- check the panel is fully dark
Step 3: sweeping vertical bar -- watch it move left to right
```

육안으로 확인할 것:
1. 화면 전체가 흰색으로 켜지는가
2. 화면 전체가 꺼지는가
3. 세로 막대가 좌우로 왕복 이동하는가

---

## 8. 문제 해결

| 증상 | 원인 / 해결 |
|---|---|
| `Display device not ready` 로그 | I2C0 배선(pin 3/4) 확인, 또는 `&i2c0`가 실제로 활성화되지 않았을 가능성 |
| 로그는 정상인데 화면이 안 켜짐 | I2C 주소 문제 — `reg = <0x3c>;`를 `<0x3d>;`로 바꿔 재시도 |
| 화면이 켜지지만 이미지가 깨져 보임(뒤집힘/밀림 등) | `multiplex-ratio`/`segment-remap`/`com-invdir` 같은 패널별 초기화 파라미터가 이 모듈과 안 맞을 수 있음 — 모듈 데이터시트 재확인 |
| 처음 전원 인가 시 보드가 불안정하거나 다른 I2C 디바이스가 오작동 | 로직 레벨 문제(4-3절)일 가능성 — 즉시 연결 해제 후 전압 재확인 |

---

## 9. TODO / VERIFY 종합

- [x] `&i2c0`의 정확한 pinctrl 노드명 — **확인 완료: `i2c0_ms_scl`/`i2c0_ms_sda`** (실사용자 확인, 2026-08)
- [x] SSD1306 devicetree `compatible` 문자열 — **확인 완료: `"solomon,ssd1306"`("fb" 없음)** (실사용자 확인, 2026-08 — 5-2절 디버깅 사례 참고)
- [x] 빌드·플래시·화면 출력 — **전체 성공 확인** (실사용자 확인, 2026-08)
- [ ] 사용하는 SSD1306 모듈의 실제 I2C 주소(0x3C vs 0x3D)
- [ ] 사용하는 모듈의 자체 풀업 저항 유무 및 그 기준 전압 (로직 레벨 안전성 직결)
- [ ] 모듈이 1.8V VDD에서 정상 동작하는지 (데이터시트 확인)

---

## 10. 참고 자료

- `ssd1306_i2c0_lab.zip` — 이 실험의 실제 코드 (CMakeLists.txt, prj.conf, app.overlay, src/main.c)
- Zephyr Display API 문서 — `zephyr/include/zephyr/drivers/display.h`
- SSD1306 데이터시트 — Solomon Systech 공식 문서 (컨트롤러 커맨드셋·타이밍 상세)
- `SC950-C01116-01` (SR110 RDK 회로도, Rev E) — 4-1절 핀맵의 근거
- `SR100 Series High-Performance AI MCUs` 제품 브리프 — 2절의 디스플레이 인터페이스 부재 근거
