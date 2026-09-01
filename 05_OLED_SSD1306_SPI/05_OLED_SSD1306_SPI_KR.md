# 4. OLED SSD1306 (0.96") — SPI 모드

## 이 실습에서 배우는 것

2번 실습(I2C 모드)과 **완전히 같은 칩(SSD1306)**을 이번엔 **SPI 모드**로 연결합니다. Zephyr의 SSD1306 드라이버는 I2C와 SPI 양쪽 버스를 하나의 드라이버가 다 지원합니다(내부적으로 `DT_ON_BUS(node_id, spi)`로 분기) — 그래서 **애플리케이션 코드는 2번과 사실상 동일**하고, 오버레이만 바뀝니다.

## 준비물

- 0.96" OLED, SSD1306, **7핀 SPI 모듈**(VCC/GND/SCK/SDA(MOSI)/RES/DC/CS)

> ⚠️ 2번에서 쓴 4핀 I2C 전용 모듈로는 이 실습이 불가능합니다. SPI 핀(특히 DC, RES)이 따로 나온 모듈인지 먼저 확인하세요.

## 배선 — 3번 실습과 같은 SPI 버스 재사용

| 신호 | ESP32-S3 연결 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCK | GPIO12 |
| SDA(MOSI) | GPIO11 |
| CS | GPIO10 |
| DC | GPIO21 |
| RES | GPIO14 |

(MOSI=11, MISO=13, SCK=12은 non-OS 커리큘럼의 SPI 실습에서부터 써온 것과 같은 핀입니다 — SSD1306은 쓰기 전용이라 MISO는 실제로 안 쓰지만, 버스 자체는 다른 SPI 장치들과 공유 가능하도록 동일하게 맞췄습니다)

## 폴더 구성

```
Zephyr_display/
└── 04_OLED_SSD1306_SPI/
    ├── lab/
    │   ├── src/
    │   │   └── main.c
    │   ├── boards/
    │   │   └── esp32s3_devkitc_esp32s3_procpu.overlay
    │   ├── CMakeLists.txt
    │   ├── prj.conf
    │   └── sample.yaml
    └── 04_OLED_SSD1306_SPI_KR.md
```

## Devicetree Overlay

```dts
&pinctrl {
    spim2_default: spim2_default {
        group1 {
            pinmux = <SPIM2_MISO_GPIO13>, <SPIM2_SCLK_GPIO12>;
        };
        group2 {
            pinmux = <SPIM2_MOSI_GPIO11>;
            output-low;
        };
    };
};

&spi2 {
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";
    pinctrl-0 = <&spim2_default>;
    pinctrl-names = "default";
    cs-gpios = <&gpio0 10 GPIO_ACTIVE_LOW>;

    oled_spi: ssd1306@0 {
        compatible = "solomon,ssd1306";
        reg = <0>;
        spi-max-frequency = <4000000>;
        dc-gpios = <&gpio0 21 GPIO_ACTIVE_HIGH>;
        reset-gpios = <&gpio0 14 GPIO_ACTIVE_LOW>;
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

/ {
    chosen {
        zephyr,display = &oled_spi;
    };
};
```

## prj.conf

```
CONFIG_SPI=y
CONFIG_DISPLAY=y
CONFIG_CHARACTER_FRAMEBUFFER=y
CONFIG_SSD1306=y
CONFIG_HEAP_MEM_POOL_SIZE=16384
```

## 코드 — 2번과 거의 동일

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <stdio.h>

#define DISPLAY_STACK_SIZE 2048
#define DISPLAY_PRIORITY   5

static void display_thread_entry(void *p1, void *p2, void *p3) {
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(dev)) {
        printk("DisplayThread: display device not ready\n");
        return;
    }

    if (display_set_pixel_format(dev, PIXEL_FORMAT_MONO10) != 0) {
        display_set_pixel_format(dev, PIXEL_FORMAT_MONO01);
    }

    if (cfb_framebuffer_init(dev)) {
        printk("DisplayThread: framebuffer init failed\n");
        return;
    }

    cfb_framebuffer_clear(dev, true);
    display_blanking_off(dev);

    printk("DisplayThread: ready (SPI mode)\n");

    int counter = 0;
    while (1) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Count: %d", counter++);

        cfb_framebuffer_clear(dev, false);
        cfb_print(dev, "SSD1306 (SPI)", 0, 0);
        cfb_print(dev, buf, 0, 16);
        cfb_framebuffer_finalize(dev);

        k_sleep(K_SECONDS(1));
    }
}

K_THREAD_DEFINE(display_id, DISPLAY_STACK_SIZE, display_thread_entry,
                NULL, NULL, NULL, DISPLAY_PRIORITY, 0, 0);

int main(void) {
    printk("main: started, DisplayThread is running independently\n");
    return 0;
}
```

## 빌드 & 실행

```powershell
west build -p always -b esp32s3_devkitc/esp32s3/procpu .\04_OLED_SSD1306_SPI\lab\
west flash
west espressif monitor
```

## 실행 & 확인

- 화면에 "SSD1306 (SPI)"와 카운터가 표시되는지 확인

## 관찰 포인트 — 2번과 나란히 비교하기

| | 2번 (I2C) | 4번 (SPI) |
|---|---|---|
| 신호선 개수 | 2개(SDA/SCL) | 4개(SCK/MOSI/CS) + DC/RES |
| 오버레이 `compatible` | `solomon,ssd1306` (I2C 바인딩) | `solomon,ssd1306` (SPI 바인딩, 같은 이름) |
| 주소 지정 | `reg = <0x3c>` (I2C 주소) | `reg = <0>` (SPI CS 인덱스) + `dc-gpios`/`reset-gpios` 추가 |
| **애플리케이션 코드(`main.c`)** | **동일** | **동일** |

**핵심은 마지막 줄입니다** — 버스가 완전히 바뀌었는데도 `main.c`는 문자열 하나("(I2C)" → "(SPI)") 빼고 똑같습니다. 이게 2/23/24번 실습에서 반복해서 강조된 **Zephyr 드라이버 모델의 추상화**가 실제로 동작하는 모습입니다. Display + CFB API를 쓰는 한, 그 아래 버스가 I2C든 SPI든 애플리케이션은 신경 쓸 필요가 없습니다.

## 트러블슈팅

| 증상 | 원인 / 해결 |
|---|---|
| 화면에 아무것도 안 나옴 | 7핀 SPI 모듈이 맞는지 확인 (4핀 I2C 전용 모듈이면 애초에 불가능) |
| `device_is_ready()`가 false | `dc-gpios`/`reset-gpios` 극성, `cs-gpios` 극성(`GPIO_ACTIVE_LOW`) 확인 |
| 3번 루프백 테스트는 통과했는데 화면이 안 나옴 | 3번은 버스 자체만 확인한 것 — DC/RES 핀은 3번 테스트에 없었으므로, 이 두 핀의 배선/극성을 별도로 확인해야 합니다 |
| I2C 모드(2번)는 됐는데 SPI 모드만 안 됨 | 같은 오버레이 안에 `&i2c0`과 `&spi2`를 동시에 활성화해뒀다면, 두 버스 모두에 `chosen { zephyr,display = ...}`을 지정할 수 없습니다(하나만 선택됨) — 지금 실습에선 SPI 쪽만 `chosen`으로 지정했는지 확인 |

## 다음

5번 실습(`05_SHARP_memory_display`)에서 SPI 기반의 또 다른 디스플레이, SHARP Memory Display를 다룹니다 — 이번엔 CS 극성이 반대(Active HIGH)인 특이 사례입니다.
