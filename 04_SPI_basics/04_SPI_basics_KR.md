# 3. SPI 기초 개념 — 왜 "SPI 스캐너"는 없는가

> **검토 노트**: 원본 파일명이 `04_SPI_basics_KR.md`로 올라왔지만, 문서 본문(제목의 "3.", 폴더 트리의 `03_SPI_basics/`, 8절의 "I2C 스캐너(1번)와 이 루프백 테스트(3번)", 마지막 절의 "4번 실습")이 전부 일관되게 이 랩을 **3번**으로 가리키고 있어서 파일명을 `03_SPI_basics_KR.md`로 맞췄습니다. Lab 01(I2C 스캐너) → Lab 02(I2C LCD) → 이 랩(Lab 03)으로 이어지는 순서와도 맞습니다.
>
> 그 외에 폴더 구성에 나열되어 있지만 본문에 내용이 없던 `CMakeLists.txt`, `sample.yaml`을 채워 넣었고, 코드/오버레이/커스텀 바인딩은 Zephyr 공식 문서·소스로 교차 확인한 결과 그대로 두어도 되는 내용이라 손대지 않았습니다. 자세한 근거는 문서 맨 아래 "검토 결과 요약"에 정리했습니다.

## 이 실습에서 배우는 것

원래 계획엔 "SPI 스캐너"가 있었는데, 검토 결과 **SPI는 구조적으로 I2C 같은 스캔이 불가능**합니다. 이 실습은 그 이유를 이해하고, 대신 **SPI 버스 자체가 정상 동작하는지 확인하는 루프백(loopback) 자가진단**을 만들어봅니다.

## I2C와 SPI, 근본적으로 다른 점

| | I2C | SPI |
|---|---|---|
| 신호선 | 2개(SDA, SCL) 공유 버스 | 최소 3~4개(SCK, MOSI, MISO, CS) |
| 장치 구분 방법 | **주소**(7비트) — 소프트웨어로 순회 가능 | **CS(Chip Select) 핀** — 어느 핀을 활성화하느냐로 결정, 배선도로만 알 수 있음 |
| "누구 있어?" 질문 | 가능 (주소에 ACK/NACK) | **불가능** — 프로토콜 자체에 주소 개념이 없음 |
| 여러 장치 연결 | 같은 두 선에 그냥 병렬 연결 | SCK/MOSI/MISO는 공유해도 되지만, 장치마다 **별도의 CS선**이 필요 |

**핵심**: I2C 스캔이 가능했던 이유는 "주소에 대고 물어보면 그 주소를 쓰는 장치만 대답한다"는 메커니즘이 있어서입니다. SPI는 애초에 이런 메커니즘이 없습니다 — CS를 활성화하는 순간 그 라인에 연결된 장치는 "내가 선택됐다"고만 인식할 뿐, 자기가 "누구인지" 먼저 알려주는 절차가 프로토콜에 없습니다. 그래서 "이 버스에 뭐가 연결되어 있지?"를 코드로 알아내는 범용적인 방법 자체가 존재하지 않습니다.

## 그럼 SPI가 제대로 연결됐는지는 어떻게 확인하나

두 단계로 나눠서 생각하면 됩니다.

1. **버스 자체(SCK/MOSI/MISO 배선, SPI 페리페럴 설정)가 정상인가** → 이건 범용적으로 확인 가능합니다. 이 실습에서 다루는 **루프백 테스트**가 이걸 확인하는 방법입니다
2. **특정 칩과 실제로 통신되는가** → 이건 그 칩 고유의 명령을 알아야만 확인 가능합니다 (예: SPI Flash 칩의 JEDEC ID 읽기 명령, 특정 디스플레이의 초기화 시퀀스 등). 5~7번 실습에서 실제 디스플레이와 통신하는 게 바로 이 단계입니다

## 루프백 테스트란

MOSI(마스터가 보내는 선)와 MISO(마스터가 받는 선)를 **물리적으로 점퍼선을 연결하거나, 아예 같은 GPIO 핀을 핀먹스 레벨에서 공유**시키면, 내가 보낸 데이터가 그대로 되돌아옵니다. 데이터가 정확히 일치하면 "적어도 SPI 페리페럴과 클럭, 핀 라우팅은 정상"이라는 걸 확인할 수 있습니다.

이번 실습은 **점퍼선 없이**, 오버레이에서 MISO와 MOSI를 같은 GPIO(8번)에 매핑하는 방식을 씁니다. ESP32-S3의 GPIO 매트릭스는 입력 라우팅과 출력 라우팅을 핀 단위로 독립적으로 설정할 수 있어서, 같은 물리 핀을 "SPI2 출력(MOSI)"이자 동시에 "SPI2 입력(MISO)"으로 쓸 수 있습니다 — 마스터가 MOSI로 내보낸 신호가 그 즉시 같은 핀에서 MISO로 읽히는 구조입니다.

## 준비물

- 별도 하드웨어 불필요 (순수 소프트웨어/핀먹스 루프백)

## 폴더 구성

```
Zephyr_display/
└── 03_SPI_basics/
    ├── lab/
    │   ├── src/
    │   │   └── main.c
    │   ├── boards/
    │   │   └── esp32s3_devkitc_esp32s3_procpu.overlay
    │   ├── dts/
    │   │   └── bindings/
    │   │       └── spi/
    │   │           └── zds,spi-loopback.yaml
    │   ├── CMakeLists.txt
    │   ├── prj.conf
    │   └── sample.yaml
    └── 03_SPI_basics_KR.md
```

## Devicetree Overlay

```dts
&pinctrl {
    spim2_loopback: spim2_loopback {
        group1 {
            pinmux = <SPIM2_MISO_GPIO8>;
            output-enable;
        };
        group2 {
            pinmux = <SPIM2_MOSI_GPIO8>;   /* MISO와 동일한 GPIO8 — 이게 핵심 */
            input-enable;
        };
        group3 {
            pinmux = <SPIM2_SCLK_GPIO12>, <SPIM2_CSEL_GPIO10>;
        };
    };
};

&spi2 {
    #address-cells = <1>;
    #size-cells = <0>;
    status = "okay";
    pinctrl-0 = <&spim2_loopback>;
    pinctrl-names = "default";

    loopback_dev: loopback@0 {
        compatible = "zds,spi-loopback";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};
```

MOSI와 MISO가 **같은 물리 핀(GPIO8)**에 매핑되어 있고, 한쪽은 `output-enable`, 다른 쪽은 `input-enable`로 설정된 게 보이시나요 — 이게 점퍼선 없이 루프백이 되는 원리입니다. SCK/CS는 각각 GPIO12/GPIO10을 씁니다 (다른 용도로 이미 쓰고 있는 핀이 아니라면 임의로 골라도 되는 자리이니, 보드 리비전에 따라 실크스크린으로 한 번 확인하는 걸 권장합니다).

## 커스텀 devicetree 바인딩

특정 실제 칩이 아니라 "버스 테스트용 자리"만 필요해서, 최소한의 바인딩만 만듭니다.

```yaml
description: Generic placeholder SPI device node, used for a bus-level loopback self-test

compatible: "zds,spi-loopback"

include: spi-device.yaml
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20.0)

# 커스텀 "zds,spi-loopback" 바인딩이 이 앱 자신의 dts/bindings/ 아래에 있으므로,
# find_package(Zephyr...)가 실행되기 전에 DTS_ROOT를 미리 확장해야 합니다.
# 순서가 바뀌면 devicetree 컴파일러가 이 바인딩을 못 찾아서
# "'zds,spi-loopback' compatible not found" 에러로 빌드가 실패합니다.
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spi_basics_lab)

target_sources(app PRIVATE src/main.c)
```

## prj.conf

```
CONFIG_SPI=y
CONFIG_GPIO=y
```

ESP32 SPI 드라이버(`CONFIG_ESP32_SPIM`)는 별도로 켜주지 않아도 됩니다 — devicetree에 `espressif,esp32-spi` 노드가 `status = "okay"`로 있으면 자동으로 `y`가 되도록 Kconfig에 정의되어 있습니다 (I2C 랩에서 `CONFIG_I2C_ESP32=y`를 명시했던 것과는 다른 부분이니 참고).

## sample.yaml

```yaml
sample:
  name: SPI basics - bus loopback self-test
  description: >
    Verify the SPI2 (GPSPI2) peripheral, clock, and pin routing on the
    ESP32-S3-DevKitC-1 using a devicetree-level loopback (MISO and MOSI
    mapped to the same GPIO pad) - no jumper wire and no real SPI device
    required.
common:
  tags:
    - spi
  platform_allow:
    - esp32s3_devkitc/esp32s3/procpu
  harness: console
  harness_config:
    type: one_line
    regex:
      - "PASS: received bytes match sent bytes.*"
tests:
  sample.spi.esp32s3_loopback:
    build_only: true
```

## 코드

```c
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <string.h>

#define LOOPBACK_NODE DT_NODELABEL(loopback_dev)

static const struct spi_dt_spec loopback_spi = SPI_DT_SPEC_GET(
    LOOPBACK_NODE, SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_OP_MODE_MASTER, 0);

static bool spi_loopback_test(void) {
    uint8_t tx_data[8] = {0x01, 0x02, 0x03, 0x04, 0xAA, 0x55, 0xFF, 0x00};
    uint8_t rx_data[8] = {0};

    struct spi_buf tx_buf = { .buf = tx_data, .len = sizeof(tx_data) };
    struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };

    struct spi_buf rx_buf = { .buf = rx_data, .len = sizeof(rx_data) };
    struct spi_buf_set rx_bufs = { .buffers = &rx_buf, .count = 1 };

    int ret = spi_transceive_dt(&loopback_spi, &tx_bufs, &rx_bufs);
    if (ret != 0) {
        printk("spi_transceive_dt failed: %d\n", ret);
        return false;
    }

    printk("Sent:     ");
    for (int i = 0; i < (int)sizeof(tx_data); i++) printk("%02X ", tx_data[i]);
    printk("\n");

    printk("Received: ");
    for (int i = 0; i < (int)sizeof(rx_data); i++) printk("%02X ", rx_data[i]);
    printk("\n");

    return memcmp(tx_data, rx_data, sizeof(tx_data)) == 0;
}

#define TEST_STACK_SIZE 2048
#define TEST_PRIORITY   5

static void test_thread_entry(void *p1, void *p2, void *p3) {
    printk("\n=== SPI Basics: bus loopback self-test ===\n");

    if (!spi_is_ready_dt(&loopback_spi)) {
        printk("SPI device not ready - check devicetree status/overlay\n");
        return;
    }

    bool ok = spi_loopback_test();

    if (ok) {
        printk("PASS: received bytes match sent bytes - SPI peripheral, "
               "clock, and pin routing are all working.\n");
    } else {
        printk("FAIL: received bytes do NOT match sent bytes - check "
               "the MISO/MOSI pinmux, or SCLK wiring.\n");
    }
}

K_THREAD_DEFINE(test_tid, TEST_STACK_SIZE, test_thread_entry,
                NULL, NULL, NULL, TEST_PRIORITY, 0, 0);

int main(void) {
    return 0;
}
```

## 빌드 & 실행

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu 03_SPI_basics/lab
west flash
west espressif monitor
```

(PowerShell이면 `west build -p always -b esp32s3_devkitc/esp32s3/procpu .\03_SPI_basics\lab\` 형태로 경로만 바꿔주면 됩니다.)

## 실행 & 확인

```
=== SPI Basics: bus loopback self-test ===
Sent:     01 02 03 04 AA 55 FF 00
Received: 01 02 03 04 AA 55 FF 00
PASS: received bytes match sent bytes - SPI peripheral, clock, and pin routing are all working.
```

`Sent`와 `Received`가 정확히 일치하는지 확인하세요.

## 관찰 포인트

- 이 테스트가 통과했다고 해서 "SPI로 아무 장치나 연결하면 다 될 것"이라는 뜻은 아닙니다 — **버스 자체(전기적 신호, 페리페럴 설정)가 정상**이라는 것만 확인된 겁니다. 실제 장치와의 통신은 그 장치의 프로토콜을 정확히 구현해야 합니다 (5~7번 실습에서 하게 될 일)
- I2C 스캐너(1번)와 이 루프백 테스트(3번)를 나란히 놓고 비교해보면, **"두 프로토콜이 겉보기엔 비슷해 보여도(클럭+데이터), 설계 철학 자체가 다르다"**는 걸 실감할 수 있습니다 — I2C는 "버스 위의 여러 장치를 발견하는" 데 최적화되어 있고, SPI는 "이미 아는 장치와 빠르게 통신하는" 데 최적화되어 있습니다
- CS 라인을 여러 개 두면(오버레이의 `cs-gpios`에 여러 GPIO를 배열로 지정) 같은 SCK/MOSI/MISO를 공유하면서 여러 SPI 장치를 연결할 수 있습니다 — 이후 실습에서 여러 디스플레이를 동시에 연결하고 싶다면 이 방식을 씁니다

## 트러블슈팅

| 증상 | 원인 / 해결 |
|---|---|
| `SPI device not ready` | 오버레이가 실제로 적용 안 됨 — 파일명이 board target과 일치하는지 확인 |
| `spi_transceive_dt failed` | SPI 버스 자체 설정 문제 — `west build -t devicetree`로 `&spi2` 노드가 제대로 병합됐는지 확인 |
| Sent와 Received가 다름 | MISO/MOSI가 정말 같은 GPIO에 매핑됐는지 오버레이 재확인 — 다른 핀으로 되어 있으면 루프백이 안 됩니다 |
| Devicetree 바인딩을 못 찾음 (`'zds,spi-loopback' compatible not found`) | `CMakeLists.txt`의 `list(APPEND DTS_ROOT ...)`가 `find_package(Zephyr...)` 이전에 있는지 확인 |
| 빌드 시 `SPIM2_MISO_GPIO8` 등 매크로를 못 찾는다는 에러 | 아래 "검토 결과 요약" 참고 — 매크로 이름 자체는 정상적인 명명 규칙을 따르고 있지만 실기 빌드로 최종 확인은 필요 |

## 다음

4번 실습(`04_OLED_SSD1306_SPI`)에서 이번에 확인한 SPI 버스 위에 실제 OLED(SSD1306 SPI 모드)를 연결합니다.

---

## 검토 결과 요약

이 문서를 전달하기 전에 다음을 Zephyr 공식 문서/소스로 교차 확인했습니다.

- **`&spi2` 라벨과 `compatible`**: ESP32-S3 SoC devicetree에 `spi2`(베이스 주소 0x60024000, GPSPI2)와 `spi3`(0x60025000, GPSPI3) 두 노드가 모두 `compatible = "espressif,esp32-spi"`, 기본 `status = "disabled"`로 존재함을 확인했습니다. 오버레이에서 `status = "okay"`로 켜는 방식은 I2C0 랩과 동일한 패턴입니다.
- **`CONFIG_ESP32_SPIM` 자동 활성화**: Zephyr의 `drivers/spi/Kconfig.esp32`에서 `ESP32_SPIM`이 `depends on DT_HAS_ESPRESSIF_ESP32_SPI_ENABLED` + `default y`로 정의되어 있음을 확인했습니다. 즉 devicetree에서 SPI 노드를 켜기만 하면 드라이버가 자동으로 활성화되므로, prj.conf에 `CONFIG_SPI=y`만 있어도 충분합니다 (I2C 랩처럼 `CONFIG_xxx_ESP32=y`를 별도로 안 넣어도 됨).
- **핀먹스 매크로 명명 규칙**: `esp32s3-pinctrl.h`에서 I2S 관련 신호들이 `<PERIPHERAL>_<SIGNAL>_GPIO<N>` 형태(예: `I2S1_MCLK_GPIO8`)로 모든 GPIO 번호에 대해 기계적으로 생성되어 있는 것을 확인했습니다. `SPIM2_MISO_GPIO8`류 매크로도 같은 규칙을 따를 가능성이 매우 높지만, 파일이 매우 커서 SPI 항목까지 직접 원문으로 확인하지는 못했습니다 — **처음 빌드할 때 이 부분만 한 번 확인**해 주세요. 매크로 이름이 틀렸다면 devicetree 컴파일 단계에서 바로 "undeclared" 류 에러로 나오기 때문에 하드웨어를 잘못 짚는 것과 달리 원인 파악이 즉각적입니다.
- **"같은 GPIO로 MOSI/MISO 매핑" 기법 자체**: ESP32/ESP32-S3 GPIO 매트릭스가 핀 단위로 입력·출력 신호 라우팅을 독립적으로 지정할 수 있다는 구조상 타당한 접근입니다. 다만 Zephyr 공식 `tests/drivers/spi/spi_loopback` 저장소에서 ESP32 계열 보드가 실제로 이 방식을 쓰는지는 저장소 탐색 제약으로 직접 확인하지 못했습니다 — 원 문서의 "Zephyr 공식 테스트에서 쓰는 기법"이라는 서술은 참고용으로만 보시고, 이 랩 자체의 정당성은 위 GPIO 매트릭스 구조로 판단하시면 됩니다.
