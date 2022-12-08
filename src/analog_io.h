#ifndef SH_RPI_FIRMWARE_SRC_ANALOG_IO_H_
#define SH_RPI_FIRMWARE_SRC_ANALOG_IO_H_

#include <Arduino.h>

#define AIN0 0x00
#define AIN1 0x01
#define AIN2 0x02
#define AIN3 0x03
#define AIN4 0x04
#define AIN5 0x05
#define AIN6 0x06
#define AIN7 0x07
#define AIN8 0x08
#define AIN9 0x09
#define AIN10 0x0A
#define AIN11 0x0B
#define ADC_TEMPSENSE 0x1E


/**
 * @brief Replacement definition of the buggy megaTinyCore init_ADC1()
 * 
 */
void init_ADC1() {
  //                              30 MHz / 32 = 937 kHz,  32 MHz / 32 =  1 MHz.
  #if   F_CPU   > 24000000     // 24 MHz / 16 = 1.5 MHz,  25 MHz / 32 =  780 kHz
    ADC1.CTRLC  = ADC_PRESC_DIV32_gc | ADC_REFSEL_VDDREF_gc | ADC_SAMPCAP_bm;
  #elif F_CPU  >= 12000000    // 16 MHz / 16 = 1.0 MHz,  20 MHz / 16 = 1.25 MHz
    ADC1.CTRLC  = ADC_PRESC_DIV16_gc | ADC_REFSEL_VDDREF_gc | ADC_SAMPCAP_bm;
  #elif F_CPU  >=  6000000    //  8 MHz /  8 = 1.0 MHz,  10 MHz /  8 = 1.25 MHz
    ADC1.CTRLC  =  ADC_PRESC_DIV8_gc | ADC_REFSEL_VDDREF_gc | ADC_SAMPCAP_bm;
  #elif F_CPU  >=  3000000    //  4 MHz /  4 = 1.0 MHz,   5 MHz /  4 = 1.25 MHz
    ADC1.CTRLC  =  ADC_PRESC_DIV4_gc | ADC_REFSEL_VDDREF_gc | ADC_SAMPCAP_bm;
  #else                       //  1 MHz /  2 = 500 kHz - the lowest setting
    ADC1.CTRLC  =  ADC_PRESC_DIV2_gc | ADC_REFSEL_VDDREF_gc | ADC_SAMPCAP_bm;
  #endif
  #if   (F_CPU == 6000000 || F_CPU == 12000000 || F_CPU == 24000000 || F_CPU ==25000000)
    ADC1.SAMPCTRL = (7); // 9 ADC clocks, 12 us
  #elif (F_CPU == 5000000 || F_CPU == 10000000 || F_CPU == 20000000)
    ADC1.SAMPCTRL = (13);   // 15 ADC clock,s 12 us
  #else
    ADC1.SAMPCTRL = (10); // 12 ADC clocks, 12 us
  #endif
  ADC1.CTRLD    = ADC_INITDLY_DLY16_gc;
  ADC1.CTRLA    = ADC_ENABLE_bm;
}

/**
 * @brief Set the analog reference for ADC0
 *
 * The implementation has been copied from the megaTinyCore Arduino Core
 * wiring_analog.c file.
 *
 * @param mode Reference mode
 */
void att1s_analog_reference_adc0(uint8_t mode) {
    switch (mode) {
      #if defined(EXTERNAL)
        case EXTERNAL:
      #endif
      case VDD:
        VREF.CTRLB &= ~VREF_ADC0REFEN_bm; // Turn off force-adc-reference-enable
        ADC0.CTRLC = (ADC0.CTRLC & ~(ADC_REFSEL_gm)) | mode | ADC_SAMPCAP_bm; // per datasheet, recommended SAMPCAP=1 at ref > 1v - we don't *KNOW* the external reference will be >1v, but it's probably more likely...
        // VREF.CTRLA does not need to be reconfigured, as the voltage references only supply their specified voltage when requested to do so by the ADC.
        break;
      case INTERNAL0V55:
        VREF.CTRLA =  VREF.CTRLA & ~(VREF_ADC0REFSEL_gm); // These bits are all 0 for 0.55v reference, so no need to do the mode << VREF_ADC0REFSEL_gp here;
        ADC0.CTRLC = (ADC0.CTRLC & ~(ADC_REFSEL_gm | ADC_SAMPCAP_bm)) | INTERNAL; // per datasheet, recommended SAMPCAP=0 at ref < 1v
        VREF.CTRLB |= VREF_ADC0REFEN_bm; // Turn off force-adc-reference-enable
        break;
      case INTERNAL1V1:
      case INTERNAL2V5:
      case INTERNAL4V34:
      case INTERNAL1V5:
        VREF.CTRLA = (VREF.CTRLA & ~(VREF_ADC0REFSEL_gm)) | (mode << VREF_ADC0REFSEL_gp);
        ADC0.CTRLC = (ADC0.CTRLC & ~(ADC_REFSEL_gm)) | INTERNAL | ADC_SAMPCAP_bm; // per datasheet, recommended SAMPCAP=1 at ref > 1v
        break;
    }
}


/**
 * @brief Set the analog reference for ADC1
 *
 * The implementation has been copied from the megaTinyCore Arduino Core
 * wiring_analog.c file.
 *
 * @param mode Reference mode
 */
void att1s_analog_reference_adc1(uint8_t mode) {
    switch (mode) {
      #if defined(EXTERNAL)
        case EXTERNAL:
      #endif
      case VDD:
        VREF.CTRLB &= ~VREF_ADC1REFEN_bm; // Turn off force-adc-reference-enable
        ADC1.CTRLC = (ADC1.CTRLC & ~(ADC_REFSEL_gm)) | mode | ADC_SAMPCAP_bm; // per datasheet, recommended SAMPCAP=1 at ref > 1v - we don't *KNOW* the external reference will be >1v, but it's probably more likely...
        // VREF.CTRLA does not need to be reconfigured, as the voltage references only supply their specified voltage when requested to do so by the ADC.
        break;
      case INTERNAL0V55:
        VREF.CTRLC =  VREF.CTRLC & ~(VREF_ADC1REFSEL_gm); // These bits are all 0 for 0.55v reference, so no need to do the mode << VREF_ADC1REFSEL_gp here;
        ADC1.CTRLC = (ADC1.CTRLC & ~(ADC_REFSEL_gm | ADC_SAMPCAP_bm)) | INTERNAL; // per datasheet, recommended SAMPCAP=0 at ref < 1v
        VREF.CTRLB |= VREF_ADC1REFEN_bm; // Turn off force-adc-reference-enable
        break;
      case INTERNAL1V1:
      case INTERNAL2V5:
      case INTERNAL4V34:
      case INTERNAL1V5:
        VREF.CTRLC = (VREF.CTRLC & ~(VREF_ADC1REFSEL_gm)) | (mode << VREF_ADC1REFSEL_gp);
        ADC1.CTRLC = (ADC1.CTRLC & ~(ADC_REFSEL_gm)) | INTERNAL | ADC_SAMPCAP_bm; // per datasheet, recommended SAMPCAP=1 at ref > 1v
        break;
    }
}


uint16_t att1s_analog_read(uint8_t ain, uint8_t adc_num) {
  volatile ADC_t *adc = adc_num == 0 ? &ADC0 : &ADC1;

  // select the analog input number
  adc->MUXPOS = ain;

  if (adc->COMMAND) return ADC_ERROR_BUSY;

  // start the conversion
  adc->COMMAND = ADC_STCONV_bm;

  // wait for the conversion to complete
  while (!(adc->INTFLAGS & ADC_RESRDY_bm)) {
    // do nothing
  }

  // return the result
  return adc->RES;
}

#endif  // SH_RPI_FIRMWARE_SRC_ANALOG_IO_H_