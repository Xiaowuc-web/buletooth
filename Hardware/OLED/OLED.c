#include "OLED.h"
#include "OLED_Font.h"

#define OLED_SCL_PIN GPIO_Pin_3
#define OLED_SDA_PIN GPIO_Pin_4

volatile uint8_t OLED_Status = 0;
volatile uint8_t OLED_DetectedAddress = 0;
static uint8_t OLED_Address = OLED_I2C_ADDRESS;

/* 缩短软件 I2C 的位延时，减小逐行刷屏的时间。 */
static void OLED_Delay(void)
{
    volatile uint32_t n = SystemCoreClock / 1500000 + 1;
    while (n != 0)
    {
        __NOP();
        --n;
    }
}

static void OLED_SCL(uint8_t level)
{
    GPIO_WriteBit(GPIOB, OLED_SCL_PIN, level ? Bit_SET : Bit_RESET);
    OLED_Delay();
}

static void OLED_SDA(uint8_t level)
{
    GPIO_WriteBit(GPIOB, OLED_SDA_PIN, level ? Bit_SET : Bit_RESET);
    OLED_Delay();
}

static void OLED_Start(void)
{
    OLED_SDA(1);
    OLED_SCL(1);
    OLED_SDA(0);
    OLED_SCL(0);
}

static void OLED_Stop(void)
{
    OLED_SDA(0);
    OLED_SCL(1);
    OLED_SDA(1);
}

/* Open-drain SDA is released for the slave's ACK bit. */
static uint8_t OLED_WriteByte(uint8_t value)
{
    uint8_t bit;
    uint8_t ack;
    for (bit = 0; bit < 8; bit++)
    {
        OLED_SDA((value & 0x80U) != 0U);
        OLED_SCL(1);
        OLED_SCL(0);
        value <<= 1;
    }
    OLED_SDA(1);
    OLED_SCL(1);
    ack = (GPIO_ReadInputDataBit(GPIOB, OLED_SDA_PIN) == Bit_RESET);
    OLED_SCL(0);
    return ack;
}

static uint8_t OLED_Begin(uint8_t control)
{
    OLED_Start();
    if (!OLED_WriteByte((uint8_t)(OLED_Address << 1)) ||
        !OLED_WriteByte(control))
    {
        OLED_Stop();
        return 0;
    }
    return 1;
}

static uint8_t OLED_Command(uint8_t command)
{
    uint8_t ack;

    if (!OLED_Begin(0x00))
    {
        return 0;
    }
    ack = OLED_WriteByte(command);
    OLED_Stop();
    return ack;
}

static uint8_t OLED_SetCursor(uint8_t x, uint8_t page)
{
    return OLED_Command(0xB0 | page) &&
           OLED_Command(0x10 | (x >> 4)) &&
           OLED_Command(x & 0x0F);
}

/**
 * @brief  初始化 SSD1306 128×64 屏及 PB3/PB4 软件 I2C。
 * @param  无。
 * @retval 1：初始化成功；0：总线、地址应答或初始化指令异常。
 * @note   关闭 JTAG，保留 PA13/PA14 的 SWD；SCL/SDA 需上拉至 3.3V。
 *         自动探测 0x3C/0x3D，清屏后开启显示；仅在启动时调用一次。
 */
uint8_t OLED_Init(void)
{
    GPIO_InitTypeDef gpio;
    uint32_t wait;
    uint8_t i;
    static const uint8_t commands[] =
    {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x30, 0xA4, 0xA6
    };

    OLED_Status = 0;
    OLED_DetectedAddress = 0;
    SystemCoreClockUpdate();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    /* Release PB3/PB4 from JTAG; PA13/PA14 SWD stays available. */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    GPIO_SetBits(GPIOB, OLED_SCL_PIN | OLED_SDA_PIN);
    gpio.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    for (wait = 0; wait < 20000U; wait++) { OLED_Delay(); }
    /* Recover an interrupted transfer before issuing STOP. */
    OLED_SDA(1);
    for (i = 0; i < 9; i++) { OLED_SCL(0); OLED_SCL(1); }
    OLED_Stop();
    if (GPIO_ReadInputDataBit(GPIOB, OLED_SCL_PIN) == Bit_RESET ||
        GPIO_ReadInputDataBit(GPIOB, OLED_SDA_PIN) == Bit_RESET)
    {
        OLED_Status = 2;
        return 0;
    }
    /* Probe both common 7-bit OLED addresses, without sending commands. */
    OLED_Address = OLED_I2C_ADDRESS;
    OLED_Start();
    i = OLED_WriteByte((uint8_t)(OLED_Address << 1));
    OLED_Stop();
    if (!i)
    {
        OLED_Address = (OLED_I2C_ADDRESS == 0x3CU) ? 0x3DU : 0x3CU;
        OLED_Start();
        i = OLED_WriteByte((uint8_t)(OLED_Address << 1));
        OLED_Stop();
    }
    if (!i) { OLED_Status = 3; return 0; }
    OLED_DetectedAddress = OLED_Address;
    OLED_Status = 4;
    if (!OLED_Begin(0x00)) { return 0; }
    for (i = 0; i < sizeof(commands); i++)
    {
        if (!OLED_WriteByte(commands[i]))
        {
            OLED_Stop();
            return 0;
        }
    }
    OLED_Stop();
    OLED_Clear();
    if (!OLED_Command(0xAF)) { return 0; }
    OLED_Status = 1;
    return 1;
}

/**
 * @brief  清除整个屏幕，直接写入 OLED 显存，无需额外刷新。
 * @param  无。
 * @retval 无。
 */
void OLED_Clear(void)
{
    uint8_t page;
    uint8_t x;
    for (page = 0; page < 8; page++)
    {
        OLED_SetCursor(0, page);
        if (!OLED_Begin(0x40)) { return; }
        for (x = 0; x < 128; x++) { OLED_WriteByte(0x00); }
        OLED_Stop();
    }
}

void OLED_ShowChinese(uint8_t x, uint8_t page, uint8_t index)
{
    uint8_t p;
    uint8_t col;
    if (x > 112 || page > 6 || index >= 3) { return; }
    for (p = 0; p < 2; p++)
    {
        OLED_SetCursor(x, (uint8_t)(page + p));
        if (!OLED_Begin(0x40)) { return; }
        for (col = 0; col < 16; col++)
        {
            OLED_WriteByte(OLED_Chinese16x16[index][p * 16 + col]);
        }
        OLED_Stop();
    }
}

/**
 * @brief  将一个 ASCII 字符写入当前数据传输，补一列空白。
 * @param  ch：字符字节；不可打印或非 ASCII 字节显示为问号。
 * @retval 1：全部字节获应答；0：传输失败。
 * @note   调用者负责设置光标、开始数据传输并发送停止信号。
 */
static uint8_t OLED_WriteGlyph(uint8_t ch)
{
    uint8_t col;

    if (ch < 0x20U || ch > 0x7EU) { ch = '?'; }
    for (col = 0; col < 5; col++)
    {
        if (!OLED_WriteByte(OLED_ASCII5x7[ch - 0x20U][col])) { return 0; }
    }
    return OLED_WriteByte(0x00);
}

/**
 * @brief  在指定页显示单行 ASCII 字符串，超出行宽的内容截断。
 * @param  x：起始像素列，0～122；page：页号，0～7，每页高 8 像素。
 * @param  str：以 '\0' 结尾的字符串，空指针不执行显示。
 * @retval 无。
 * @note   字符占 6×8 像素，不自动换行、不清除字符串后方旧内容。
 *         直接写屏；通信失败后将 OLED_Status 设为 5，并停止写屏。
 */
void OLED_ShowString(uint8_t x, uint8_t page, const char *str)
{
    if (OLED_Status != 1 || str == 0 || x > 122 || page > 7) { return; }
    if (GPIO_ReadInputDataBit(GPIOB, OLED_SCL_PIN) == Bit_RESET ||
        GPIO_ReadInputDataBit(GPIOB, OLED_SDA_PIN) == Bit_RESET ||
        !OLED_SetCursor(x, page) || !OLED_Begin(0x40))
    {
        OLED_Status = 5;
        return;
    }
    while (*str != '\0' && x <= 122)
    {
        if (!OLED_WriteGlyph((uint8_t)*str))
        {
            OLED_Status = 5;
            break;
        }
        str++;
        x = (uint8_t)(x + 6U);
    }
    OLED_Stop();
}

/**
 * @brief  更新串口接收内容区域，同时擦除上一条内容，保留顶部标题。
 * @param  str：原接收缓冲区，以 '\0' 结尾；空指针不执行显示。
 * @retval 无。
 * @note   页 0 保留标题、页 1 留空；页 2～7 每行显示 20 个 6×8 字符，
 *         左侧留 6 像素边距，自动换行，最多显示 120 字节，超出部分截断。
 *         每页直接覆盖全部 128 列，未使用位置写零，不残留旧字符串。
 *         不改变接收缓冲区；不支持通用中文，非 ASCII 字节显示为问号。
 */
void OLED_ShowReceived(const char *str)
{
    uint8_t page;
    uint8_t col;
    uint8_t cell;
    uint8_t ch;

    if (str == 0) { return; }
    for (page = 2; page < 8; page++)
    {
        OLED_SetCursor(0, page);
        if (!OLED_Begin(0x40)) { return; }
        for (col = 0; col < 6; col++)
        {
            if (!OLED_WriteByte(0x00)) { OLED_Stop(); return; }
        }
        for (cell = 0; cell < 20; cell++)
        {
            ch = ' ';
            if (*str != '\0')
            {
                ch = (uint8_t)*str;
                str++;
            }
            if (!OLED_WriteGlyph(ch)) { OLED_Stop(); return; }
        }
        for (col = 0; col < 2; col++)
        {
            if (!OLED_WriteByte(0x00)) { OLED_Stop(); return; }
        }
        OLED_Stop();
    }
}

void OLED_ShowName(void)
{
    /* Three 16x16 characters, centered at x=40, y=24. */
    OLED_ShowChinese(40, 3, 0);
    OLED_ShowChinese(56, 3, 1);
    OLED_ShowChinese(72, 3, 2);
}
