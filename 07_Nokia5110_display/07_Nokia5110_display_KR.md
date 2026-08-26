# 7. Nokia 5110 (PCD8544) 모노크롬 LCD — 84x48, raw SPI

## 개요

보드는 **ESP32-S3-DevKitC-1** (`esp32s3_devkitc/esp32s3/procpu`), 프레임워크는 **Zephyr RTOS**입니다.

옛날 노키아 휴대폰 액정으로 유명한 **PCD8544 컨트롤러 기반 84x48 모노크롬 LCD**("Nokia 5110" 모듈)를 raw SPI로 구동합니다. Zephyr Display/CFB 서브시스템은 쓰지 않고, 이 시리즈의 다른 랩들(SSD1306, I2C LCD, SPI 루프백)과 동일하게 애플리케이션 코드에서 직접 SPI/GPIO를 다룹니다.

이 모듈은 **MISO 라인이 아예 없습니다** (write-only 디스플레이) — 그래서 3번 랩(SPI 루프백)과 달리 이번 오버레이는 MOSI/SCLK/CS만 있으면 됩니다.

> ✅ **실기 검증 완료**: 실제 ESP32-S3-DevKitC-1 + Nokia 5110으로 확인했고, 기본 Vop(`0xB0`) 그대로 정상 표시됨(별도 대비 조정 불필요). 검증 과정에서 실제로 걸렸던 문제는 코드/오버레이가 아니라 **배선**이었습니다 — 이전에 다른 배선(다른 GPIO)으로 테스트했던 걸 그대로 재사용하다가, 이번 랩의 배선표(RST=GPIO4, DC=GPIO5, CE=GPIO10, DIN=GPIO11, CLK=GPIO12)와 실제 연결이 어긋나 있었습니다. SPI는 I2C와 달리 ACK/NACK이 없어서(3번 랩 참고) 배선이 틀려도 시리얼 로그엔 에러가 안 뜨고 그냥 "화면에 아무것도 안 보임"으로만 나타난다는 점, 기억해두면 다음 SPI 디스플레이 랩에서도 유용합니다.

## 준비물

- Nokia 5110 (PCD8544) LCD 모듈 (보통 8핀: RST, CE, DC, DIN, CLK, VCC, LIGHT, GND)
- ESP32-S3-DevKitC-1

## 배선

| 신호 | 역할 | ESP32-S3 연결 |
|---|---|---|
| VCC | 전원 | 3.3V |
| GND | 그라운드 | GND |
| RST | 리셋 (active low) | GPIO4 |
| CE (CS) | 칩 선택 | GPIO10 (SPI2 하드웨어 CS0) |
| DC | Data/Command 선택 | GPIO5 |
| DIN (MOSI) | 데이터 입력 | GPIO11 |
| CLK (SCLK) | 클럭 | GPIO12 |
| LIGHT (BL) | 백라이트 | 3.3V 또는 GND 스위칭 (선택) |

> **전원 관련 좋은 소식**: PCF8574 LCD(Lab 02)나 SSD1306과 달리, Nokia 5110/PCD8544 모듈은 원래 노키아 폰 내부 부품이라 **네이티브 로직 레벨이 2.7~3.3V**입니다. ESP32-S3(3.3V 전용 GPIO)와 전압 궁합이 좋아서, 레벨 시프터나 5V 관련 고민 없이 바로 연결하면 됩니다. 다만 모듈에 따라 온보드 레귤레이터/저항 분배망이 5V 입력을 가정하고 만들어진 경우도 있으니(중국산 저가 브레이크아웃 보드 중 일부), VCC 핀에 5V를 물려도 되는지는 각자 모듈의 실크스크린/판매처 설명을 한 번 확인하세요. 이 랩은 **3.3V 직결을 기준**으로 작성했습니다.

## PCD8544 명령 체계

PCD8544는 "basic instruction set"과 "extended instruction set" 두 모드를 오가며 설정합니다.

| 명령 | 값 | 의미 |
|---|---|---|
| Function Set (extended) | `0x21` | extended 모드로 진입 |
| Set Vop (contrast) | `0x80 \| Vop` | 명암 설정 — 이 랩은 기본값 `0xB0` 사용 |
| Temperature Control | `0x04` | 온도 계수 0 |
| Bias System | `0x14` | bias 1:48 |
| Function Set (basic) | `0x20` | basic 모드로 복귀 |
| Display Control | `0x0C` | 정상(비반전) 표시 모드 |

**대비(Vop) 값은 모듈마다 편차가 큽니다.** HD44780 LCD(Lab 02)에는 물리 트리머가 있어서 손으로 돌리면 됐지만, PCD8544는 **트리머가 없고 이 Vop 명령 값이 곧 소프트웨어 콘트라스트**입니다. 화면이 안 보이거나(너무 연함) 전부 까맣게 나오면(너무 진함) `main.c`의 `PCD8544_SET_VOP_DEFAULT`(기본 `0xB0`)를 `0x80`~`0xFF` 범위에서 조정하세요.

## 주소 지정과 프레임버퍼

- 화면은 84 x 48 픽셀 = 가로 84칼럼 x 세로 6페이지(페이지당 8픽셀)
- `0x80|x` 로 X(칼럼) 주소, `0x40|y` 로 Y(페이지) 주소를 지정
- 주소 지정 후 데이터를 계속 흘려보내면 **X가 자동 증가하다가 84에서 다음 페이지로 자동 줄바꿈**됩니다 — 그래서 전체 프레임버퍼(84x6=504바이트)를 (0,0)에서 시작해 한 번에 전부 써버리는 것으로 화면 전체를 갱신할 수 있습니다

## 코드 구조

- `pcd8544_cmd()` / `pcd8544_data()`: DC 핀을 0(명령)/1(데이터)로 설정한 뒤 `spi_write_dt()`로 전송 (CE/CS는 SPI 드라이버가 전송 구간에 맞춰 자동으로 로우/하이)
- `pcd8544_init()`: RST 펄스 → extended 명령들(Vop/온도/bias) → basic 복귀 → 정상 표시 모드
- `framebuffer[84*6]`: 화면 전체를 담는 배열, `fb_draw_char`/`fb_draw_string`으로 5x7 폰트를 채워 넣고 `pcd8544_update()`로 한 번에 전송
- `main()`: SPI/GPIO 준비 확인 → 초기화 → "Hello World!" / "Nokia 5110" 두 줄 출력

## Devicetree

```dts
&pinctrl {
    spim2_pcd8544: spim2_pcd8544 {
        group1 {
            pinmux = <SPIM2_MOSI_GPIO11>, <SPIM2_SCLK_GPIO12>, <SPIM2_CSEL_GPIO10>;
        };
    };
};

&spi2 {
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";
    pinctrl-0 = <&spim2_pcd8544>;
    pinctrl-names = "default";

    pcd8544: pcd8544@0 {
        compatible = "zds,pcd8544";
        reg = <0>;
        spi-max-frequency = <1000000>;
        reset-gpios = <&gpio0 4 GPIO_ACTIVE_LOW>;
        dc-gpios = <&gpio0 5 GPIO_ACTIVE_HIGH>;
    };
};
```

`reset-gpios`/`dc-gpios` 속성 이름은 Zephyr 자체의 `mipi-dbi-spi` 디스플레이 바인딩이 쓰는 이름과 동일하게 맞췄습니다 — 이 랩은 그 프레임워크를 쓰지 않고 커스텀 바인딩(`zds,pcd8544`)이지만, 이름 관례는 공식 바인딩과 통일해두는 게 나중에 실제 Zephyr 디스플레이 드라이버로 갈아탈 때도 헷갈리지 않습니다.

GPIO 컨트롤러 라벨은 `&gpio0`을 썼습니다 — ESP32-S3의 devicetree GPIO 컨트롤러는 `gpio0`(핀 0~31 담당)과 `gpio1`(핀 32~53 담당) 두 개로 나뉘어 있는데, 이 랩에서 쓰는 GPIO4/5는 둘 다 `gpio0` 범위입니다.

## 커스텀 devicetree 바인딩

```yaml
description: |
  PCD8544-based Nokia 5110 monochrome LCD (84x48), driven over raw SPI
  from application code - no Zephyr Display/CFB subsystem involved.

compatible: "zds,pcd8544"

include: spi-device.yaml

properties:
  reset-gpios:
    type: phandle-array
    required: true
    description: >
      Reset pin. Active low - pulse low to reset the PCD8544 controller.

  dc-gpios:
    type: phandle-array
    required: true
    description: >
      Data/Command select pin. Driven low before a command byte is
      written, high before a data byte is written.
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20.0)

list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nokia5110_lab)

target_sources(app PRIVATE src/main.c)
```

(3번 랩과 동일하게, 커스텀 바인딩을 쓰기 때문에 `DTS_ROOT`를 `find_package(Zephyr...)`보다 먼저 추가해야 합니다.)

## prj.conf

```
CONFIG_SPI=y
CONFIG_GPIO=y
CONFIG_PRINTK=y
```

(3번 랩에서 확인한 대로, `CONFIG_ESP32_SPIM`은 devicetree에서 SPI 노드가 켜지면 자동으로 `y`가 되므로 별도로 안 넣어도 됩니다.)

## 폴더 구성

```
Zephyr_display/
└── 07_Nokia5110_display/
    ├── lab/
    │   ├── src/
    │   │   └── main.c
    │   ├── boards/
    │   │   └── esp32s3_devkitc_esp32s3_procpu.overlay
    │   ├── dts/
    │   │   └── bindings/
    │   │       └── display/
    │   │           └── zds,pcd8544.yaml
    │   ├── CMakeLists.txt
    │   ├── prj.conf
    │   └── sample.yaml
    └── 07_Nokia5110_display_KR.md
```

## Build & Run

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu 07_Nokia5110_display/lab
west flash
west espressif monitor
```

### 예상 시리얼 출력

```
Nokia 5110 (PCD8544) lab starting
Nokia 5110 initialized and "Hello World!" / "Nokia 5110" written
```

화면 1페이지(맨 위)에 `Hello World!`, 2페이지에 `Nokia 5110`이 표시되어야 합니다.

## 관찰 포인트

- **Vop(대비) 값이 이 랩에서 가장 조정이 필요할 가능성이 높은 부분**입니다 — 물리 트리머가 없는 대신 소프트웨어 값이라, 화면이 안 보인다고 배선부터 의심하기 전에 `PCD8544_SET_VOP_DEFAULT` 값부터 몇 가지 바꿔보는 걸 권장합니다
- MISO가 없는 write-only 디스플레이라는 점에서 3번 랩(SPI 루프백)과 좋은 대조가 됩니다 — 루프백 테스트로 검증했던 "버스 자체가 정상"이라는 전제 위에, 이번엔 실제 장치의 명령 프로토콜을 얹는 실습입니다
- `reset-gpios`/`dc-gpios`처럼 Zephyr 공식 바인딩과 이름을 맞추는 습관은, 나중에 이 커스텀 드라이버를 진짜 Zephyr Display 드라이버로 옮길 때 devicetree를 거의 그대로 재사용할 수 있게 해줍니다

## 트러블슈팅

| 증상 | 원인 / 해결 |
|---|---|
| `SPI device not ready` / `RST/DC GPIO not ready` | 오버레이 미적용 — 파일명이 board target과 일치하는지 확인 |
| 화면이 완전히 흰색(또는 아무 표시 없음) | Vop가 너무 낮음(대비 부족) — `PCD8544_SET_VOP_DEFAULT`를 `0xB0`에서 조금씩 올려보기(`0xB8`, `0xC0` 등) |
| 화면이 전부 까맣게 나옴/체커보드 패턴 | Vop가 너무 높음(대비 과함) — 값을 낮춰보기, 또는 RST 시퀀스가 제대로 안 됐는지 확인 |
| 시리얼에 init 실패 로그, SPI write 에러 | 배선(특히 CE/CS, DIN/MOSI, CLK) 재확인. `west build -t devicetree`로 `&spi2` 노드 병합 확인 |
| 글자가 알아볼 수 없이 깨짐/위치가 이상함 | `fb_draw_string`의 6픽셀 간격 로직, 또는 페이지(page) 인자가 0~5 범위인지 확인 |
| 컴파일 에러 (`SPIM2_MOSI_GPIO11` 등을 못 찾음) | 1번/3번 랩과 동일한 이슈 — `west build -t devicetree`로 실제 사용 가능한 핀먹스 매크로 확인 |
| `'zds,pcd8544' compatible not found` | `CMakeLists.txt`의 `list(APPEND DTS_ROOT ...)`가 `find_package(Zephyr...)` 이전에 있는지 확인 |

## 다음

이후 실습에서는 SHARP 메모리 LCD, ST7735 컬러 TFT 등 다른 디스플레이 컨트롤러로 계속 확장할 수 있습니다 — 이번 랩의 raw SPI + 커스텀 바인딩 패턴을 그대로 재사용하면 됩니다.
