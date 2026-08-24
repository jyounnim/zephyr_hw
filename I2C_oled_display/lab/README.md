# SSD1306 OLED — I2C0 (J25 pin 3/4) Bring-up Lab

**대상**: SR110 RDK, Zephyr (`sr100_rdk/sr100/m55`)
**목적**: 온보드에 아무것도 안 물려있던 **I2C0 버스**를 새로 활성화해서, 외부 SSD1306 OLED(128×64)를 구동

## 검증 상태 — ✅ 실사용자 확인, 끝까지 빌드·동작 성공 (2026-08)

- `&i2c0` 자체는 `sr100_rdk_m55.dts`/`m4.dts` 어디에도 활성화돼 있지 않음(둘 다 `&i2c1`만 사용) — 이 lab에서 **처음으로 I2C0을 켜는** 것
- pinctrl 노드명은 `sr100_pinctrl.dtsi`에서 직접 확인됨: **`i2c0_ms_scl`**, **`i2c0_ms_sda`** — I2C1처럼 "_b"류 접미사가 붙지 않음. `app.overlay`에 반영 완료
- **`compatible` 문자열 정정 (중요, 실사용자 확인됨)**: `"solomon,ssd1306fb"`가 아니라 **`"solomon,ssd1306"`**("fb" 없음)이 이 Zephyr 버전(v4.4.1)의 정확한 값입니다. 잘못된 문자열을 쓰면 `Kconfig.ssd1306`의 `depends on DT_HAS_SOLOMON_SSD1306_ENABLED` 조건이 거짓이 되어 `CONFIG_SSD1306` 자체가 **`.config`에 나타나지도 않고**(꺼진 게 아니라 메뉴에서 아예 숨겨짐), 링크 단계에서 `undefined reference to __device_dts_ord_N` 에러가 남 — 드라이버가 없는 게 아니라 devicetree 문자열 불일치가 원인이었음
- 위 두 가지 반영 후 빌드·플래시·화면 출력까지 전부 실사용자 확인 완료

## 하드웨어 연결 (J25, 회로도 실측 확인됨)

| SSD1306 모듈 | J25 |
|---|---|
| VCC | Pin 1 또는 19 (1.8V) — ⚠️ 모듈 자체 풀업이 3.3V에 물려있는지 먼저 확인 (README 하단 참고) |
| GND | Pin 2 또는 20 |
| SCL | **Pin 3** (I2C0_MS_SCL) |
| SDA | **Pin 4** (I2C0_MS_SDA) |

## 파일 구성

```
ssd1306_i2c0_lab/
├── CMakeLists.txt
├── prj.conf
├── app.overlay      # I2C0 활성화 + SSD1306 노드 (pinctrl 이름 TODO/VERIFY)
└── src/main.c        # 흰 화면 → 검은 화면 → 좌우로 움직이는 바 순서로 검증
```

## 빌드

**실행 위치**: PowerShell, 워크스페이스 루트

```powershell
west build -p always -b sr100_rdk/sr100/m55 <이 폴더 경로>
```

## 플래시

지금까지 쓰시던 것과 동일합니다 (`srsdk_tools`의 `srsdk_image_generator.py` + `openocd_flash.py`).

## 실행 확인 (J14, 115200bps)

**기대 로그**:
```
SSD1306 I2C0 lab starting
Display ready: 128x64
Step 1: full white screen -- check the panel is lit solid white
Step 2: full black screen -- check the panel is fully dark
Step 3: sweeping vertical bar -- watch it move left to right
```

**육안 확인**:
1. 화면 전체가 흰색으로 켜지는지 (2초)
2. 화면 전체가 꺼지는지 (2초)
3. 세로 막대가 좌우로 왔다갔다 움직이는지 (계속 반복)

3단계까지 정상이면, 단순 우연한 풀스크린 fill이 아니라 **display_write()의 좌표/pitch 계산까지 정확히 동작**한다는 증거입니다.

## 문제 해결

| 증상 | 원인 / 해결 |
|---|---|
| `Display device not ready` | I2C0 배선(pin 3/4) 확인, 또는 `&i2c0`가 실제로는 안 켜졌을 가능성 |
| 빌드 시 `undefined reference to __device_dts_ord_N` (링커 에러) | `compatible`이 `"solomon,ssd1306fb"`로 되어 있을 가능성 — **`"solomon,ssd1306"`(fb 없음)이 정답** (실사용자 확인됨). `.config`에 `CONFIG_SSD1306` 항목 자체가 안 보이면 이 문제일 확률이 매우 높음 |
| 로그는 정상인데 화면이 안 켜짐 | I2C 주소 문제 — `reg = <0x3c>;`를 `<0x3d>;`로 바꿔서 재시도 |
| 화면이 켜지긴 하는데 이상하게 깨져 보임 | `multiplex-ratio`/`segment-remap`/`com-invdir` 등 패널별 초기화 파라미터가 이 모듈과 안 맞을 수 있음 — 모듈 데이터시트 재확인 |

## ⚠️ 로직 레벨 주의 (재확인)

SR110 GPIO는 **1.8V 전용**입니다. SSD1306 모듈이 자체 풀업 저항을 3.3V(모듈 자신의 VCC)에 물려서 나온 제품이면, 그 상태로 연결 시 I2C 버스 전체가 3.3V로 끌려 올라가 SR110 GPIO에 과전압이 걸릴 수 있습니다. 아래 중 하나로 해결하세요:
1. 모듈 VCC를 1.8V로 공급 (모듈이 1.8V 동작 지원하는지 데이터시트 확인)
2. 모듈의 자체 풀업 저항 제거
3. 레벨 시프터 사용
