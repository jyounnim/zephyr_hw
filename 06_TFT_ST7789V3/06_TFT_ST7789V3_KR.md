# Lab 06: TFT ST7789V3 (1.69인치 240x280, raw SPI)

## 1. 개요

보드는 **ESP32-S3-DevKitC-1** (`esp32s3_devkitc/esp32s3/procpu`), 프레임워크는 **Zephyr RTOS**를 사용합니다.

Sitronix **ST7789V3** 컨트롤러를 쓰는 1.69인치 240x280 컬러 TFT 패널을 SPI로 구동합니다. Zephyr의 Display/CFB 서브시스템이나 in-tree `sitronix,st7789v` 드라이버를 쓰지 않고, 이 시리즈의 다른 SPI 디스플레이 랩과 동일하게 **raw SPI**로 컨트롤러를 직접 제어합니다.

## 2. 배선

이 랩은 이 시리즈의 ST7735 랩(08번)과 **동일한 SPI2 핀 배정**을 그대로 재사용합니다 — 브레드보드를 새로 배선할 필요 없이, ST7735 랩에서 쓰던 배선 그대로 모듈만 바꿔 끼우면 됩니다.

| 신호 | 역할 | ESP32-S3-DevKitC-1 |
|---|---|---|
| VCC | 전원 | 3.3V |
| GND | 그라운드 | GND |
| SCL/SCLK | SPI 클럭 | GPIO14 (SPI2 SCLK) |
| SDA/MOSI | SPI 데이터 | GPIO13 (SPI2 MOSI) |
| CS | 칩 셀렉트 | GPIO15 (SPI2 하드웨어 CS0) |
| RES/RST | 리셋 | GPIO16 |
| DC | Data/Command 선택 | GPIO17 |
| BLK | 백라이트 | 3.3V (아래 참고) |

MISO는 배선하지 않습니다 — 이 패널은 MCU 쪽에서 보면 write-only입니다.

**BLK(백라이트) 핀**: 모듈에 BLK 핀이 별도로 나와 있다면 3.3V에 연결해야 백라이트가 켜집니다 — GPIO로 제어할 필요 없이 전원에 그대로 묶으면 됩니다. 모듈에 따라 백라이트가 보드 내부에서 이미 VCC에 연결되어 있어 BLK 핀 자체가 없거나 별도 배선 없이 켜지는 경우도 있습니다. 손에 든 모듈에 BLK 핀이 있는데 화면이 안 켜지거나 어둡다면 이 핀부터 확인하십시오.

## 3. 패널 GRAM 오프셋

ST7789 컨트롤러 자체의 네이티브 GRAM은 240x320입니다. 이 랩에서 쓰는 1.69인치 모듈의 실제 유리 패널은 240x280이라, 컨트롤러 GRAM의 가운데 부분만 잘라서 씁니다 — 그래서 주소 창(addressing window)을 지정할 때마다 고정된 오프셋을 더해줘야 합니다.

이 랩은 X 오프셋 0, Y 오프셋 20을 기본값으로 씁니다(1.69인치 240x280 ST7789V3 모듈에서 흔히 문서화되는 값). 오프셋은 오버레이의 `x-offset`/`y-offset` 프로퍼티와 `main.c`의 `X_OFFSET`/`Y_OFFSET`(둘 다 devicetree에서 읽어옴)에 있으니, 화면이 밀려 보이거나 잘려 보이면 여기를 조정하면 됩니다 (자세한 건 트러블슈팅 문서 참고).

## 4. Devicetree

이 랩은 Zephyr 공식 디스플레이 드라이버를 쓰지 않으므로, `zds,st7789v`라는 커스텀 바인딩을 하나 만들어 배선 정보(SPI 버스/CS, RST/DC 핀)와 패널 스펙(해상도, 오프셋)을 devicetree에 담아둡니다. 이 시리즈의 Lab 07(`zds,pcd8544`)/Lab 08(`zds,st7735`)과 같은 패턴입니다.

```dts
&pinctrl {
    spim2_default: spim2_default {
        group1 {
            pinmux = <SPIM2_SCLK_GPIO14>, <SPIM2_MOSI_GPIO13>, <SPIM2_CSEL_GPIO15>;
        };
    };
};

&spi2 {
    status = "okay";
    pinctrl-0 = <&spim2_default>;
    pinctrl-names = "default";

    st7789v_disp: st7789v@0 {
        compatible = "zds,st7789v";
        reg = <0>;
        spi-max-frequency = <20000000>;
        reset-gpios = <&gpio0 16 GPIO_ACTIVE_LOW>;
        dc-gpios = <&gpio0 17 GPIO_ACTIVE_HIGH>;
        width = <240>;
        height = <280>;
        x-offset = <0>;
        y-offset = <20>;
    };
};
```

`main.c`는 `SPI_DT_SPEC_GET()` / `GPIO_DT_SPEC_GET()`으로 이 노드에서 SPI 스펙과 RST/DC GPIO 스펙을 그대로 가져다 씁니다 — 핀 번호나 SPI 클럭을 코드에 하드코딩하지 않습니다.

## 5. 코드 구조

- `st7789_write_cmd()` / `st7789_write_data()`: DC 핀을 명령/데이터 모드로 바꾼 뒤 `spi_write_dt()`로 1바이트(명령) 또는 N바이트(데이터)를 보냅니다. CS는 SPI2 하드웨어 CS0이 트랜잭션마다 자동으로 토글해줍니다.
- `st7789_reset()`: RST 핀을 assert(로우) → deassert(하이)로 펄스하고, 컨트롤러가 리셋에서 복구할 시간을 기다립니다.
- `st7789_init()`: 표준 ST7789 초기화 시퀀스 — Software Reset → Sleep Out → COLMOD(16bpp RGB565) → MADCTL(방향/색상 순서) → Display Inversion On → Normal Display Mode On → Display On.
- `st7789_set_addr_window()`: CASET/RASET/RAMWR로 픽셀을 쓸 사각 영역을 지정합니다. X_OFFSET/Y_OFFSET을 여기서 한 곳에 더해주므로, 이 함수를 쓰는 다른 모든 함수는 오프셋을 신경 쓸 필요가 없습니다.
- `st7789_fill_rect()`: 지정한 사각형을 단색으로 채웁니다. 화면 전체 크기의 프레임버퍼를 RAM에 들고 있지 않고, 한 줄(row)치 버퍼만 만들어서 필요한 줄 수만큼 반복 전송합니다 — 이 시리즈의 다른 raw-SPI 디스플레이 랩과 동일한 방식입니다.
- `st7789_draw_char()` / `st7789_draw_string()`: 5x7 비트맵 폰트를 `scale`배 확대해서 그립니다. 문자 하나도 한 줄씩 전송하며, 별도의 문자 버퍼를 두지 않습니다.
- `main()`: 리셋 → 초기화 → 검은 배경 채우기 → 무지개색 색상 막대 채우기 → `"Hello World!"` 텍스트 출력.

## 6. 빌드 & 실행

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu 06_TFT_ST7789V3/lab
west flash
west espressif monitor
```

### 예상 시리얼 출력

```
=== TFT ST7789V3 (SPI, 240x280) ===
ST7789V3 initialized, color bars + "Hello World!" drawn
```

패널 화면 상단에 `Hello World!`가, 그 아래에 빨강/초록/파랑/노랑/시안/마젠타/흰색 색상 막대가 채워져 있어야 합니다.

## 7. 참고

이 랩은 실기(ESP32-S3-DevKitC-1 + ST7789V3 1.69인치 240x280 모듈)에서 정상 동작이 확인되었습니다. 화면이 밀리거나 색이 이상하게 나오는 등, 사용 중인 모듈 개체에 따라 증상이 다를 수 있는 부분은 별도 트러블슈팅 문서(`06_TFT_ST7789V3_TROUBLESHOOTING_KR.md`)를 참고해 주세요.

## 8. 파일 구성

```
06_TFT_ST7789V3/
├── 06_TFT_ST7789V3_KR.md
├── 06_TFT_ST7789V3_EN.md
├── 06_TFT_ST7789V3_TROUBLESHOOTING_KR.md
├── 06_TFT_ST7789V3_TROUBLESHOOTING_EN.md
└── lab/
    ├── CMakeLists.txt
    ├── README.rst
    ├── prj.conf
    ├── sample.yaml
    ├── boards/
    │   └── esp32s3_devkitc_esp32s3_procpu.overlay
    ├── dts/
    │   └── bindings/
    │       └── display/
    │           └── zds,st7789v.yaml
    └── src/
        └── main.c
```
