#include "grove_bme280.h"
#include "board_pins.h"

/*
 * Driver du BME280 (température, humidité, pression) sur I2C1.
 * Le capteur du montage est un BME280 (ID 0x60), pas le BME680 prévu par le dépôt CESI.
 * Les formules de compensation sont celles de la fiche technique Bosch (BST-BME280-DS002, partie 4.2.3).
 */

extern I2C_HandleTypeDef GROVE_I2C_HANDLE;

#define BME280_CHIP_ID      0x60
#define BME280_REG_ID       0xD0
#define BME280_REG_CALIB1   0x88    /* dig_T1 à dig_H1 : 0x88 à 0xA1 */
#define BME280_REG_CALIB2   0xE1    /* dig_H2 à dig_H6 : 0xE1 à 0xE7 */
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG   0xF5
#define BME280_REG_DATA     0xF7    /* pression, température, humidité : 0xF7 à 0xFE */

static uint16_t bme280_addr = (0x76 << 1);

/* Coefficients de calibration, propres à chaque puce, écrits en usine */
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3;
static int16_t  dig_H2, dig_H4, dig_H5;
static int8_t   dig_H6;

/* Température fine, calculée avec la température et réutilisée pour la pression et l'humidité */
static int32_t t_fine;

static bool read_regs(uint8_t reg, uint8_t *data, uint16_t len)
{
    return HAL_I2C_Mem_Read(&GROVE_I2C_HANDLE, bme280_addr, reg, I2C_MEMADD_SIZE_8BIT, data, len, 100) == HAL_OK;
}

static bool write_reg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&GROVE_I2C_HANDLE, bme280_addr, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 100) == HAL_OK;
}

static bool read_calibration(void)
{
    uint8_t c1[26];
    uint8_t c2[7];

    if (!read_regs(BME280_REG_CALIB1, c1, sizeof(c1))) return false;
    if (!read_regs(BME280_REG_CALIB2, c2, sizeof(c2))) return false;

    /* Octet de poids faible en premier */
    dig_T1 = (uint16_t)(c1[1] << 8 | c1[0]);
    dig_T2 = (int16_t)(c1[3] << 8 | c1[2]);
    dig_T3 = (int16_t)(c1[5] << 8 | c1[4]);
    dig_P1 = (uint16_t)(c1[7] << 8 | c1[6]);
    dig_P2 = (int16_t)(c1[9] << 8 | c1[8]);
    dig_P3 = (int16_t)(c1[11] << 8 | c1[10]);
    dig_P4 = (int16_t)(c1[13] << 8 | c1[12]);
    dig_P5 = (int16_t)(c1[15] << 8 | c1[14]);
    dig_P6 = (int16_t)(c1[17] << 8 | c1[16]);
    dig_P7 = (int16_t)(c1[19] << 8 | c1[18]);
    dig_P8 = (int16_t)(c1[21] << 8 | c1[20]);
    dig_P9 = (int16_t)(c1[23] << 8 | c1[22]);
    dig_H1 = c1[25];    /* 0xA1 (0xA0 n'est pas utilisé) */

    dig_H2 = (int16_t)(c2[1] << 8 | c2[0]);
    dig_H3 = c2[2];
    dig_H4 = (int16_t)((int8_t)c2[3] * 16 | (c2[4] & 0x0F));
    dig_H5 = (int16_t)((int8_t)c2[5] * 16 | (c2[4] >> 4));
    dig_H6 = (int8_t)c2[6];

    return true;
}

bool GroveBME280_Init(void)
{
    uint8_t id = 0;

    /* Adresse 0x76 si la broche SDO est à GND, 0x77 si elle est à VCC */
    bme280_addr = (0x76 << 1);
    if (!read_regs(BME280_REG_ID, &id, 1) || id != BME280_CHIP_ID)
    {
        bme280_addr = (0x77 << 1);
        if (!read_regs(BME280_REG_ID, &id, 1) || id != BME280_CHIP_ID)
            return false;
    }

    if (!read_calibration()) return false;

    /*
     * Mode normal : le capteur mesure seul, toutes les secondes.
     * Lire une valeur revient alors à lire des registres, sans attendre : main() ne bloque pas.
     * Suréchantillonnage x1 partout, pas de filtre.
     */
    if (!write_reg(BME280_REG_CTRL_HUM, 0x01)) return false;   /* humidité x1 (à écrire avant CTRL_MEAS) */
    if (!write_reg(BME280_REG_CONFIG, 0xA0)) return false;     /* attente 1000 ms entre deux mesures, filtre éteint */
    if (!write_reg(BME280_REG_CTRL_MEAS, 0x27)) return false;  /* température x1, pression x1, mode normal */

    return true;
}

/* Résultat en centièmes de °C (5123 = 51,23 °C). Calcule aussi t_fine. */
static int32_t compenser_temperature(int32_t adc_T)
{
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12)
                    * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

/* Résultat en Pa x 256 (24674867 = 96386,2 Pa). */
static uint32_t compenser_pression(int32_t adc_P)
{
    int64_t var1 = ((int64_t)t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;

    if (var1 == 0) return 0;    /* évite une division par zéro */

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);

    return (uint32_t)p;
}

/* Résultat en % x 1024 (47445 = 46,333 %). */
static uint32_t compenser_humidite(int32_t adc_H)
{
    int32_t v = t_fine - ((int32_t)76800);

    v = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v)) + ((int32_t)16384)) >> 15)
         * (((((((v * ((int32_t)dig_H6)) >> 10) * (((v * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10)
              + ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
    v = (v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
    v = (v < 0) ? 0 : v;
    v = (v > 419430400) ? 419430400 : v;

    return (uint32_t)(v >> 12);
}

bool GroveBME280_ReadEnvironment(Env_Data *data)
{
    if (data == 0) return false;

    uint8_t r[8];
    if (!read_regs(BME280_REG_DATA, r, sizeof(r))) return false;

    int32_t adc_P = (int32_t)(((uint32_t)r[0] << 12) | ((uint32_t)r[1] << 4) | (r[2] >> 4));
    int32_t adc_T = (int32_t)(((uint32_t)r[3] << 12) | ((uint32_t)r[4] << 4) | (r[5] >> 4));
    int32_t adc_H = (int32_t)(((uint32_t)r[6] << 8) | r[7]);

    /* 0x80000 : valeur de démarrage, aucune mesure n'est encore prête */
    if (adc_T == 0x80000) return false;

    /* La température d'abord : elle calcule t_fine, utilisé par les deux autres */
    data->temperature_c    = compenser_temperature(adc_T) / 100.0f;
    data->pressure_hpa     = compenser_pression(adc_P) / 25600.0f;   /* /256 pour les Pa, /100 pour les hPa */
    data->humidity_percent = compenser_humidite(adc_H) / 1024.0f;
    data->gas_resistance_ohm = 0.0f;    /* le BME280 n'a pas de capteur de gaz */

    return true;
}
