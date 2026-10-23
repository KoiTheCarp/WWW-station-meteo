#include "boutons.h"
#include "board_pins.h"

#define ROUGE_PORT  BUTTON1_PORT
#define ROUGE_PIN   BUTTON1_PIN
#define VERT_PORT   BUTTON2_PORT
#define VERT_PIN    BUTTON2_PIN

#define ETAT_ENFONCE    GPIO_PIN_RESET

#define APPUI_LONG_MS   5000U
#define ANTI_REBOND_MS  30U

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;

    volatile uint32_t debut_appui_ms;
    volatile uint32_t dernier_front_ms;
    volatile bool enfonce;
    volatile bool deja_signale;
} Bouton;

static Bouton rouge = { ROUGE_PORT, ROUGE_PIN, 0U, 0U, false, false };
static Bouton vert  = { VERT_PORT,  VERT_PIN,  0U, 0U, false, false };

void Boutons_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = ROUGE_PIN;
    HAL_GPIO_Init(ROUGE_PORT, &gpio);

    gpio.Pin = VERT_PIN;
    HAL_GPIO_Init(VERT_PORT, &gpio);

    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2U, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    HAL_NVIC_SetPriority(EXTI3_IRQn, 2U, 0U);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}

void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BUTTON1_PIN);
}

void EXTI3_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BUTTON2_PIN);
}

static void Bouton_Front(Bouton *b)
{
    uint32_t maintenant = HAL_GetTick();

    if ((maintenant - b->dernier_front_ms) < ANTI_REBOND_MS)
    {
        return;
    }
    b->dernier_front_ms = maintenant;

    if (HAL_GPIO_ReadPin(b->port, b->pin) == ETAT_ENFONCE)
    {
        b->debut_appui_ms = maintenant;
        b->enfonce = true;
        b->deja_signale = false;
    }
    else
    {
        b->enfonce = false;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == rouge.pin)
    {
        Bouton_Front(&rouge);
    }
    else if (GPIO_Pin == vert.pin)
    {
        Bouton_Front(&vert);
    }
}

static bool Bouton_AppuiLong(Bouton *b)
{
    bool enfonce;
    uint32_t debut;

    __disable_irq();
    enfonce = b->enfonce;
    debut = b->debut_appui_ms;
    __enable_irq();

    if (!enfonce || b->deja_signale)
    {
        return false;
    }

    __disable_irq();
    if (HAL_GPIO_ReadPin(b->port, b->pin) != ETAT_ENFONCE)
    {
        b->enfonce = false;
        __enable_irq();
        return false;
    }
    __enable_irq();

    if ((HAL_GetTick() - debut) < APPUI_LONG_MS)
    {
        return false;
    }

    b->deja_signale = true;
    return true;
}

bool Boutons_RougeEnfonce(void)
{
    return HAL_GPIO_ReadPin(ROUGE_PORT, ROUGE_PIN) == ETAT_ENFONCE;
}

bool Boutons_RougeAppuiLong(void)
{
    return Bouton_AppuiLong(&rouge);
}

bool Boutons_VertAppuiLong(void)
{
    return Bouton_AppuiLong(&vert);
}
