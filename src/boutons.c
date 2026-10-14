#include "boutons.h"
#include "board_pins.h"

/*
 * Quel bouton du Grove Dual Button est le rouge, lequel est le vert ?
 * À vérifier sur la carte, puis inverser ces lignes si besoin.
 */
#define ROUGE_PORT  BUTTON1_PORT    /* PA10, ligne EXTI10 */
#define ROUGE_PIN   BUTTON1_PIN
#define VERT_PORT   BUTTON2_PORT    /* PB3, ligne EXTI3 */
#define VERT_PIN    BUTTON2_PIN

/*
 * État de la broche quand le bouton est enfoncé.
 * Le driver du CESI considère le bouton enfoncé à l'état bas. À vérifier sur la carte.
 */
#define ETAT_ENFONCE    GPIO_PIN_RESET

#define APPUI_LONG_MS   5000U   /* durée imposée par la spécification */
#define ANTI_REBOND_MS  30U     /* un front à moins de 30 ms du précédent est un rebond */

/* Tout ce qu'il faut savoir sur un bouton, regroupé dans une structure */
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;

    /* Écrits par l'interruption, lus par main() : donc volatile */
    volatile uint32_t debut_appui_ms;   /* heure de l'appui (HAL_GetTick) */
    volatile uint32_t dernier_front_ms; /* heure du dernier front gardé (anti-rebond) */
    volatile bool enfonce;              /* le bouton est-il en train d'être tenu ? */
    volatile bool deja_signale;         /* l'appui long a-t-il déjà été signalé ? */
} Bouton;

static Bouton rouge = { ROUGE_PORT, ROUGE_PIN, 0U, 0U, false, false };
static Bouton vert  = { VERT_PORT,  VERT_PIN,  0U, 0U, false, false };

void Boutons_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Interruption sur les deux fronts : à l'appui ET au relâchement */
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_PULLUP;    /* broche à 1 au repos, à 0 quand on appuie */
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = ROUGE_PIN;
    HAL_GPIO_Init(ROUGE_PORT, &gpio);

    gpio.Pin = VERT_PIN;
    HAL_GPIO_Init(VERT_PORT, &gpio);

    /* On allume les deux voyants du NVIC : ligne 10 -> groupe 15_10, ligne 3 -> la sienne */
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2U, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

    HAL_NVIC_SetPriority(EXTI3_IRQn, 2U, 0U);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}

/* Gestionnaires d'interruption : ils appellent le "gardien" de la HAL */
void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BUTTON1_PIN);
}

void EXTI3_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BUTTON2_PIN);
}

/*
 * Appelée par l'interruption à chaque front.
 * Elle reste courte : elle note l'heure et l'état, rien d'autre.
 */
static void Bouton_Front(Bouton *b)
{
    uint32_t maintenant = HAL_GetTick();

    /* Anti-rebond : on ignore un front trop proche du précédent */
    if ((maintenant - b->dernier_front_ms) < ANTI_REBOND_MS)
    {
        return;
    }
    b->dernier_front_ms = maintenant;

    if (HAL_GPIO_ReadPin(b->port, b->pin) == ETAT_ENFONCE)
    {
        /* Front d'appui : on note l'heure de début */
        b->debut_appui_ms = maintenant;
        b->enfonce = true;
        b->deja_signale = false;
    }
    else
    {
        /* Front de relâchement */
        b->enfonce = false;
    }
}

/* Callback commun à toutes les broches : on regarde qui a sonné */
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

/*
 * Appelée par main(). Vrai une seule fois quand le bouton est tenu depuis 5 s.
 * Le mode change au bout de 5 s, sans attendre que l'utilisateur lâche le bouton.
 */
static bool Bouton_AppuiLong(Bouton *b)
{
    bool enfonce;
    uint32_t debut;

    /* Copie cohérente : l'interruption ne peut pas modifier ces valeurs pendant qu'on les lit */
    __disable_irq();
    enfonce = b->enfonce;
    debut = b->debut_appui_ms;
    __enable_irq();

    if (!enfonce || b->deja_signale)
    {
        return false;
    }

    /* Sécurité contre un rebond mal filtré : le bouton est-il vraiment encore enfoncé ? */
    if (HAL_GPIO_ReadPin(b->port, b->pin) != ETAT_ENFONCE)
    {
        return false;
    }

    if ((HAL_GetTick() - debut) < APPUI_LONG_MS)
    {
        return false;
    }

    b->deja_signale = true;     /* un seul changement de mode par appui */
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
