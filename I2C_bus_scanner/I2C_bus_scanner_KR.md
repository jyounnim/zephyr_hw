# I2C Bus Scanner — Zephyr (SR110)

부팅 시 별도 스레드에서 I2C0 → I2C1 순서로 한 번 스캔하고,
각 버스에서 ACK을 보내는 디바이스를 찾아 `i2cdetect` 스타일의 그리드로
출력하는 예제입니다. 새 센서/모듈을 보드에 연결했을 때 어떤 주소에
잡히는지 확인하는 용도로 씁니다.

## 폴더 구성

`zephyr_hw` 디렉토리 하위에 위치하는 것을 기준으로 합니다.

```
<west workspace root>/
└── zephyr_hw/
    └── I2C_bus_scanner/
        ├── lab/
        │   ├── src/
        │   │   └── main.c              # 스캐너 로직 (주석 영문)
        │   ├── boards/
        │   │   └── sr100_rdk_sr100_m55.overlay   # I2C0 활성화용 오버레이 (필수)
        │   ├── CMakeLists.txt
        │   ├── prj.conf
        │   ├── README.rst              # Zephyr 공식 샘플 형식 문서 (영문)
        │   └── sample.yaml             # twister 테스트 메타데이터
        ├── I2C_bus_scanner_KR.md       # 본 문서
        └── I2C_bus_scanner_EN.md       # 영문 버전
```

## 동작 방식

1. `K_THREAD_DEFINE`으로 정의된 전용 스레드(`scan_tid`)가 부팅 시 자동 시작되어
   I2C0 → I2C1 순서로 스캔 수행 (main 스레드는 아무 일도 안 하고 바로 반환)
2. 각 주소(0x08~0x77)에 1바이트 **read**를 시도해 성공(ACK) 여부로 디바이스 존재 판단
3. 스캔 종료 후 발견된 디바이스 개수와 주소 맵 출력
4. 한 번 스캔하고 스레드 종료 (반복 없음)

## Build

west workspace 루트(`zephyr_hw`의 부모 디렉토리)에서 실행합니다.

오버레이 파일명(`boards/sr100_rdk_sr100_m55.overlay`)이 board target
(`sr100_rdk/sr100/m55` → 슬래시를 밑줄로 치환)과 정확히 일치하기 때문에,
Zephyr 빌드 시스템이 `-DEXTRA_DTC_OVERLAY_FILE` 없이 자동으로 찾아서
적용합니다. I2C0는 SoC devicetree 기본값이 `status = "disabled"`라
이 오버레이가 빠지면 링크 에러가 납니다. (I2C1은 보드 dts에서 이미
활성화되어 있어 오버레이가 필요 없습니다.)

```powershell
west build -p always -b sr100_rdk/sr100/m55 .\zephyr_hw\I2C_bus_scanner\lab\
```

> `-DEXTRA_DTC_OVERLAY_FILE=...`을 추가로 붙이지 마세요 — 파일명이 이미
> 자동 인식 규칙과 일치해서 중복 지정이 되고, CMake 인자 처리 과정에서
> devicetree 전처리가 실패합니다.

### 관련 devicetree 정보

- `sr100_m55.dtsi:268-277`에서 I2C0는 `status = "disabled"`, pinctrl 지정 없음
- `sr100_m55.dtsi:280-...` / `sr100_rdk_m55.dts:141-144`에서 I2C1은 이미
  `status = "okay"` + `pinctrl-0 = <&i2c1_ms_scl_b &i2c1_ms_sda_b>`로 활성화되어
  있고, PCA6416A GPIO 익스팬더(`gpio_exp0`)도 이미 그 위에 붙어 있음
- I2C0용 핀 그룹은 `sr100_pinctrl.dtsi:586,603`에 `i2c0_ms_sda` / `i2c0_ms_scl`로
  정의되어 있으나 `/omit-if-no-ref/`라 아무 데서도 참조하지 않으면 빌드에서
  아예 빠져서 `status = "disabled"`인 채로 남음 → 오버레이로 참조해줘야 함

## Flash

`srsdk_tools`로 이동해서 `openocd_flash.py`로 진행합니다.

```powershell
cd C:\02.work\syna_zephry\syna_zephry_sdk\srsdk_tools
python openocd_flash.py `
  --openocd C:\02.work\SRSDK_Build_tools\OpenOCD\xpack-openocd-0.12.0-4\bin\openocd.exe `
  --flash-offset 0x0 `
  --file-offset 0x0 `
  --cfg_path Input_Config\sr100_m55.cfg `
  --image Output\B0_Flash\B0_flash_full_image_GD25LE128_67Mhz_secured.bin
```

> `--image` 경로는 이번 `I2C_bus_scanner` 빌드로 새로 생성된 secured image
> 경로로 바뀌어야 합니다.

플래시 완료 후 콘솔(230400bps 8N1)을 열면 입력 없이 바로 한 번 스캔합니다.

## 결과 예

```
=== I2C Bus Scanner (SR110) ===

Scanning I2C0...
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- 3d -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- --
Scan complete on I2C0: 1 device(s) found
Scanning I2C1...
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: 20 -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- 32 -- -- -- -- -- -- 39 -- -- -- -- -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- 4c -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- --
Scan complete on I2C1: 4 device(s) found
```

## 확인 완료된 항목

- [x] `DT_NODELABEL(i2c0)` / `DT_NODELABEL(i2c1)` 라벨명 — `sr100_m55.dtsi:268,280` 확인
- [x] I2C0/I2C1 기본 `status` 값 — I2C0는 disabled(오버레이 필요), I2C1은 okay(불필요)
- [x] I2C0용 pinctrl 그룹명 — `i2c0_ms_scl` / `i2c0_ms_sda` (`sr100_pinctrl.dtsi:586,603`)
- [x] 오버레이 자동 적용 — 파일명이 board target(`sr100_rdk/sr100/m55` → `sr100_rdk_sr100_m55`)
      과 일치해서 `-DEXTRA_DTC_OVERLAY_FILE` 없이 자동 인식됨
- [x] **1바이트 `i2c_read`** probe 방식 사용 — 실보드 검증 레퍼런스 코드(`i2c_test`)
      기준으로 확인된 방식. `CONFIG_I2C_DW=y`도 명시적으로 추가
- [x] `K_THREAD_DEFINE`으로 전용 스레드를 만들어 부팅 시 한 번만 스캔하고
      종료하는 구조 (`main()`은 빈 채로 반환)

## TODO / VERIFY (아직 확인 필요)

- [ ] I2C0 클럭 속도 100kHz(standard mode)가 실제 연결할 디바이스에 적합한지 확인
      (400kHz fast mode가 필요하면 오버레이의 `clock-frequency` 값 수정)
- [ ] I2C0 핀(`i2c0_ms_scl`/`i2c0_ms_sda`)이 보드 커넥터의 어느 핀에 나오는지
      보드 회로도/핀아웃 문서로 확인 (다른 예제와 핀 충돌 가능성)
- [ ] `openocd_flash.py`용 secured image를 만드는 정확한 signing/packaging
      절차 (`build\zephyr\zephyr.bin` → `Output\B0_Flash\...` 변환 과정) 확인
