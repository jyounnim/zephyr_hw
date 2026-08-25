# 1. I2C Bus Scanner — Zephyr (ESP32-S3)

부팅 시 별도 스레드에서 I2C0을 한 번 스캔하고, ACK를 보내는 디바이스를
찾아 `i2cdetect` 스타일의 그리드로 출력하는 예제입니다. 새 센서/디스플레이
모듈을 보드에 연결했을 때 어떤 주소에 잡히는지 확인하는 용도로 씁니다.
`Zephyr_display` 프로젝트의 2번(OLED I2C), 이후 SHARP/Nokia/ST7735
실습에서 배선을 확인할 때도 계속 재사용합니다.

## 폴더 구성

```
Zephyr_display/
└── 01_I2C_bus_scanner/
    ├── lab/
    │   ├── src/
    │   │   └── main.c              # 스캐너 로직 (주석 영문)
    │   ├── boards/
    │   │   └── esp32s3_devkitc_esp32s3_procpu.overlay   # I2C0 활성화 오버레이
    │   ├── CMakeLists.txt
    │   ├── prj.conf
    │   └── sample.yaml
    └── 01_I2C_bus_scanner_KR.md     # 본 문서
```

## I2C 버스 개념 정리

| 개념 | 설명 |
|---|---|
| 버스 공유 | SDA/SCL 두 선에 여러 장치가 병렬로 매달림 |
| 주소 | 각 장치가 7비트 주소(0x08~0x77 범위)를 가짐 — 이게 스캔이 가능한 이유 |
| ACK/NACK | 마스터가 주소를 보내면, 그 주소를 쓰는 장치만 ACK로 응답. 아무도 없으면 NACK |
| 스캔 원리 | "이 주소에 누구 있어?"를 물어볼 표준 명령은 없지만, **주소만 보내고 데이터 없이 끝내는(zero-length) 트랜잭션**을 순회하면서 ACK 여부만 확인하면 어떤 장치든 존재 여부를 알 수 있음 |

## 배선

I2C의 두 신호선 이름은 **SDA**(Serial **DA**ta, 데이터)와 **SCL**(Serial **CL**ock, 클럭)입니다.

> ⚠️ **SCK/SCLK는 SPI 용어입니다.** I2C의 클럭 라인은 항상 **SCL**이라고 부릅니다 — 모듈 실크스크린에 "SCK"나 "CLK"로 인쇄된 경우도 있어 헷갈리기 쉬운데, I2C 모듈(핀이 보통 VCC/GND/SDA/SCL 4개)이라면 그 핀이 곧 SCL입니다.

| 신호 | 역할 | ESP32-S3 연결 (이 실습 오버레이 기준) |
|---|---|---|
| VCC | 전원 | 3.3V |
| GND | 그라운드 | GND |
| **SDA** | 데이터 | **GPIO8** |
| **SCL** | 클럭 (SPI의 SCK에 해당) | **GPIO9** |

이 두 핀(GPIO8/9)은 ESP32-S3 Arduino 프레임워크의 I2C 기본 핀(`Wire.begin()` 기본값)과 동일하게 맞춘 값입니다 — non-OS 커리큘럼에서 쓰던 것과 같은 핀이라, 이미 그쪽 배선을 해두셨다면 그대로 재사용 가능합니다. 오버레이의 `pinmux` 값을 바꾸면 다른 핀으로도 옮길 수 있습니다.

```dts
pinmux = <I2C0_SDA_GPIO8>, <I2C0_SCL_GPIO9>;
```

## 동작 방식

1. `K_THREAD_DEFINE`으로 정의된 전용 스레드(`scan_tid`)가 부팅 시 자동 시작되어 I2C0 스캔 수행 (`main()`은 아무 일도 안 하고 바로 반환)
2. 각 주소(0x08~0x77)에 **길이 0짜리 write**를 시도해 ACK 여부로 디바이스 존재 판단
3. 스캔 종료 후 발견된 디바이스 개수와 주소 맵 출력
4. 한 번 스캔하고 스레드 종료 (반복 없음)

## 프로빙 방식 — 왜 read가 아니라 zero-length write인가

처음엔 각 주소에 **1바이트 read**를 시도하는 방식으로 만들었었는데, 실제 보드에서 **간혹 존재하는 디바이스를 못 찾는** 현상이 있었습니다. 원인을 찾아보니 두 가지였습니다.

1. **일부 I2C 장치는 사전에 레지스터 주소를 write해주지 않으면 read 자체가 의미 있게 응답하지 않습니다** — "현재 포인터"가 이전 상태에 따라 달라지는 장치는 read 결과가 실행마다 달라질 수 있습니다
2. **Zephyr GitHub 이슈 #45008**("esp32: i2c_read() error was returned successfully at the bus nack")에 정확히 이 문제가 보고되어 있습니다 — ESP32 계열 I2C 드라이버에서 `i2c_read()`의 NACK 감지가 완전히 신뢰할 수 없다는 내용입니다

**해결**: Zephyr 공식 샘플(`samples/drivers/i2c/i2c_scanner`)과 동일하게, **길이 0짜리 write**로 프로빙하도록 바꿨습니다. 이 방식은 "주소 바이트에 대한 ACK만" 확인하고 그 이상 아무것도 요구하지 않아서, 장치의 내부 상태와 무관하게 훨씬 안정적입니다.

```c
static bool i2c_probe_addr(const struct device *bus, uint8_t addr)
{
    int ret = i2c_write(bus, NULL, 0, addr);
    return (ret == 0);
}
```

## Devicetree — I2C0 활성화

ESP32-S3 보드의 I2C0은 기본적으로 `status = "disabled"`인 경우가 많아, 오버레이로 켜줘야 합니다.

```dts
&pinctrl {
    i2c0_default: i2c0_default {
        group1 {
            pinmux = <I2C0_SDA_GPIO8>, <I2C0_SCL_GPIO9>;
            bias-pull-up;
            drive-open-drain;
            output-high;
        };
    };
};

&i2c0 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_STANDARD>;
    pinctrl-0 = <&i2c0_default>;
    pinctrl-names = "default";
};
```

> ⚠️ **확인 필요**: 사용 중인 ESP32-S3 보드 정의 파일이 `i2c0_default` pinctrl을 이미 갖고 있을 수도 있습니다 (`status`만 disabled인 채로). 그 경우 위 `&pinctrl` 블록은 빼고 `&i2c0` 블록만 남기면 됩니다. `west build -t devicetree` 후 생성되는 `build/zephyr/zephyr.dts`에서 `i2c0_default`가 이미 정의되어 있는지 먼저 확인하세요. GPIO8/GPIO9는 ESP32-S3 Arduino 프레임워크의 I2C 기본 핀과 동일하게 맞춘 예시 값입니다 — 실제 배선에 맞게 조정하세요.

## Build

```powershell
west build -p always -b esp32s3_devkitc/esp32s3/procpu .\01_I2C_bus_scanner\lab\
west flash
west espressif monitor
```

## 결과 예

```
=== I2C Bus Scanner (ESP32-S3) ===

Scanning I2C0...
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:                         -- -- -- -- -- -- -- --
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
30: -- -- -- -- -- -- -- -- -- -- -- -- -- 3c -- --
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
50: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --
70: -- -- -- -- -- -- -- --
Scan complete on I2C0: 1 device(s) found
```

(위 예시는 0x3C에 SSD1306 OLED 하나만 연결된 경우입니다 — 2번 실습에서 실제로 이 상태를 만들게 됩니다)

## 관찰 포인트

- 이 스캐너는 **`Zephyr_display` 프로젝트 전체에서 반복 재사용할 진단 도구**입니다 — 이후 실습에서 "장치가 응답 안 함" 같은 문제가 생기면, 가장 먼저 이 스캐너부터 돌려서 실제로 그 주소가 잡히는지 확인하는 습관을 들이세요
- read 프로빙에서 write 프로빙으로 바꾼 이번 수정은, **"공식 샘플과 다르게 구현하면 그 이유가 있는 경우가 많다"**는 걸 보여주는 사례입니다 — 뭔가 이상하게 동작하면 공식 샘플/문서와 내 구현이 어디서 갈라지는지부터 비교해보는 게 좋은 디버깅 습관입니다
- 스캔 결과에 예상 밖의 주소가 나오면, 그 자체로 유용한 정보입니다 — 배선 오류(다른 장치가 잘못 붙음), 혹은 알고 있던 것과 다른 주소를 쓰는 모듈(핀 점퍼로 주소가 바뀌는 경우 등)일 수 있습니다

## 트러블슈팅

| 증상 | 원인 / 해결 |
|---|---|
| `[I2C0] device not ready` | 오버레이가 실제로 적용 안 됨 — 오버레이 파일명이 board target과 일치하는지, 아니면 `-DEXTRA_DTC_OVERLAY_FILE`로 명시했는지 확인 |
| 아무 주소도 안 잡힘 | 배선(SDA/SCL) 확인, 풀업 저항 확인, `pinmux` 값이 실제 연결한 핀과 맞는지 확인 |
| 특정 장치만 간헐적으로 빠짐 | 아직도 이 문제가 있다면 `i2c_probe_addr`이 zero-length write로 바뀌었는지 재확인 — read 방식으로 되돌아가 있으면 다시 이 문제가 재현될 수 있습니다 |
| 컴파일 에러 (`I2C0_SDA_GPIO8` 등을 못 찾음) | ESP32 pinctrl 헤더가 include 안 됨 — 보드의 기본 오버레이/dtsi가 이미 처리해주는 경우가 많으니, 이 심볼이 다른 이름일 수도 있습니다. `west build -t devicetree`로 실제 사용 가능한 핀먹스 매크로 확인 |

## 다음

2번 실습(`02_OLED_SSD1306_I2C`)에서 이 스캐너로 확인한 배선 위에 실제 OLED 디스플레이를 올립니다.
