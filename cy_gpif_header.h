#ifndef   _CY_GPIF_HEADER_H_
#define   _CY_GPIF_HEADER_H_
#include "cy_lvds.h"

/* The source file was generated at this timestamp: 2026-04-24 09:04:57 */


#if defined(__cplusplus)
extern "C" {
#endif
/* Summary
   Number of states in the state machine */
#define CY_NUMBER_OF_STATES 6
#define START 0
#define IDLE0 1
#define READ0 2
#define STOP 3
#define IDLE1 4
#define READ1 5
/* Summary
   Initial value of early outputs from the state machine */
#define ALPHA_START 0x00


cy_stc_lvds_gpif_reg_data_t cy_lvds_gpif0_reg_data[] = {
{0x00000000, 0x000480F0}, /* GPIF_CONFIG */
{0x00000004, 0x00002000}, /* GPIF_BUS_CONFIG */
{0x00000008, 0x00000000}, /* GPIF_BUS_CONFIG2 */
{0x0000000C, 0x00600202}, /* GPIF_AD_CONFIG */
{0x00000010, 0x03020100}, /* GPIF_CTL_FUNC0 */
{0x00000014, 0x07060504}, /* GPIF_CTL_FUNC1 */
{0x00000018, 0x0B0A0908}, /* GPIF_CTL_FUNC2 */
{0x0000001C, 0x0F0E0D0C}, /* GPIF_CTL_FUNC3 */
{0x00000020, 0x13121110}, /* GPIF_CTL_FUNC4 */
{0x00000050, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_0 */
{0x00000054, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_1 */
{0x00000058, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_2 */
{0x0000005C, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_3 */
{0x00000060, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_4 */
{0x00000064, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_5 */
{0x00000068, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_6 */
{0x0000006C, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_7 */
{0x00000070, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_8 */
{0x00000074, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_9 */
{0x00000078, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_10 */
{0x0000007C, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_11 */
{0x00000080, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_12 */
{0x00000084, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_13 */
{0x00000088, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_14 */
{0x0000008C, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_15 */
{0x00000090, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_16 */
{0x00000094, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_17 */
{0x00000098, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_18 */
{0x0000009C, 0x00000000}, /* GPIF_CTRL_BUS_DIRECTION_19 */
{0x000000B0, 0x00000000}, /* GPIF_CTRL_BUS_DEFAULT */
{0x000000B4, 0x00000000}, /* GPIF_CTRL_BUS_POLARITY */
{0x000000B8, 0x00000000}, /* GPIF_CTRL_BUS_TOGGLE */
{0x00000100, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_0 */
{0x00000104, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_1 */
{0x00000108, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_2 */
{0x0000010C, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_3 */
{0x00000110, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_4 */
{0x00000114, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_5 */
{0x00000118, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_6 */
{0x0000011C, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_7 */
{0x00000120, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_8 */
{0x00000124, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_9 */
{0x00000128, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_10 */
{0x0000012C, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_11 */
{0x00000130, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_12 */
{0x00000134, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_13 */
{0x00000138, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_14 */
{0x0000013C, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_15 */
{0x00000140, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_16 */
{0x00000144, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_17 */
{0x00000148, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_18 */
{0x0000014C, 0x00000000}, /* GPIF_CTRL_BUS_SELECT_19 */
{0x00000160, 0x00000000}, /* GPIF_CTRL_COUNT_CONFIG */
{0x00000164, 0x00000000}, /* GPIF_CTRL_COUNT_RESET */
{0x00000168, 0x00000000}, /* GPIF_CTRL_COUNT_LIMIT */
{0x00000170, 0x00000000}, /* GPIF_ADDR_COUNT_CONFIG */
{0x00000174, 0x00000000}, /* GPIF_ADDR_COUNT_RESET */
{0x00000178, 0x00000000}, /* GPIF_ADDR_COUNT_LIMIT */
{0x00000190, 0x0000010B}, /* GPIF_DATA_COUNT_CONFIG */
{0x00000194, 0x00000000}, /* GPIF_DATA_COUNT_RESET_LSB */
{0x0000019C, 0x00000000}, /* GPIF_DATA_COUNT_RESET_MSB */
{0x000001A0, 0x00000001}, /* GPIF_DATA_COUNT_LIMIT_LSB */
{0x000001A4, 0x00000000}, /* GPIF_DATA_COUNT_LIMIT_MSB */
{0x000001A8, 0x00000000}, /* GPIF_CTRL_COMP_VALUE */
{0x000001AC, 0x00000000}, /* GPIF_CTRL_COMP_MASK */
{0x000001B0, 0x00000000}, /* GPIF_DATA_COMP_VALUE_WORD */
{0x000001D0, 0x00000000}, /* GPIF_DATA_COMP_MASK_WORD */
{0x000001E0, 0x00000000}, /* GPIF_ADDR_COMP_VALUE */
{0x000001E4, 0x00000000}, /* GPIF_ADDR_COMP_MASK */
{0x00000210, 0x00000000}, /* GPIF_CRC_CALC_CONFIG */
{0x00000218, 0xFFFFFFFF}, /* GPIF_BETA_DEASSERT */
{0x000002B0, 0x00000000}, /* LINK_IDLE_CFG */
{0x000002C0, 0x00000100}, /* LVCMOS_CLK_OUT_CFG */
};



/*  Summary
    Transition function values used in the state machine.
 */
uint16_t cy_lvds_gpif0_transition[] ={
    0x0000, 0xAAAA, 0x5555, 0xCCCC
};

/*  Summary
    Table that maps state indices to the descriptor table indices.
 */
uint8_t cy_lvds_gpif0_wavedata_position[] ={
    0, 1, 2, 0, 3, 4
};



/*  Summary
    Table containing the transition information for various states.
    This table has to be stored in the WAVEFORM Registers.
    This array consists of non - replicated waveform descriptors and acts as a
    waveform table.
 */
const cy_stc_lvds_gpif_wavedata_t cy_lvds_gpif0_wavedata[] ={
    {{0x1E73DA01, 0x00000106, 0x80000080, 0x00000000}, {0x00000000, 0x00000000, 0x00000000, 0x00000000}},
    {{0x1E73D402, 0x20000006, 0x80000040, 0x00000000}, {0x2E739E03, 0x00000000, 0x80000080, 0x00000000}},
    {{0x1E73DA04, 0x04000106, 0x80000080, 0x00000000}, {0x2E739E03, 0x00000000, 0x80000080, 0x00000000}},
    {{0x1E73D405, 0x24000006, 0x80000040, 0x00000000}, {0x2E739E03, 0x00000000, 0x80000080, 0x00000000}},
    {{0x1E73DA01, 0x00000106, 0x80000080, 0x00000000}, {0x2E739E03, 0x00000000, 0x80000080, 0x00000000}}
};


const cy_stc_lvds_gpif_config_t cy_lvds_gpif0_config = {
    (uint16_t)(sizeof(cy_lvds_gpif0_wavedata_position) / sizeof(uint8_t)),
    (cy_stc_lvds_gpif_wavedata_t *)cy_lvds_gpif0_wavedata,
    cy_lvds_gpif0_wavedata_position,
    (uint16_t)(sizeof(cy_lvds_gpif0_transition) / sizeof(uint16_t)),
    cy_lvds_gpif0_transition,
    (uint16_t)(sizeof(cy_lvds_gpif0_reg_data) / sizeof(cy_stc_lvds_gpif_reg_data_t)),
    (cy_stc_lvds_gpif_reg_data_t *)cy_lvds_gpif0_reg_data
};

/* LVDS PHY configuration output generated by Device Configurator */
cy_stc_lvds_phy_config_t cy_lvds_phy0_config ={
    .wideLink            = 0u,  
    .modeSelect          = CY_LVDS_PHY_MODE_LVCMOS,  
    .lvcmosClkMode       = CY_LVDS_LVCMOS_CLK_MASTER,  
    .dataBusWidth        = CY_LVDS_PHY_LVCMOS_MODE_NUM_LANE_16,  
    .gearingRatio        = CY_LVDS_PHY_GEAR_RATIO_1_1,  
    .clkSrc              = CY_LVDS_GPIF_CLOCK_LVCMOS_IF,  
    .clkDivider          = CY_LVDS_GPIF_CLOCK_DIV_INVALID,  
    .lvcmosMasterClkSrc  = CY_LVDS_MASTER_CLK_SRC_USB2,  
    .interfaceClock      = CY_LVDS_PHY_INTERFACE_CLK_80_MHZ,  
    .slaveFifoMode       = CY_LVDS_NORMAL_MODE,  
    .ctrlBusBitMap       = 0x00000000,  
    .dataBusDirection    = CY_LVDS_PHY_AD_BUS_DIR_INPUT,  
    .loopbackModeEn      = false,  
    .isPutLoopbackMode   = false,  
    .phyTrainingPattern  = 0x00,  
    .linkTrainingPattern = 0x00000000,  
    .interfaceClock_kHz  = 0x00000000  
};

cy_stc_lvds_config_t cy_lvds0_config ={
    .phyConfig           = (cy_stc_lvds_phy_config_t *)&cy_lvds_phy0_config,  
    .gpifConfig          = &cy_lvds_gpif0_config  
};



#if defined(__cplusplus)
}
#endif

#endif    /* _CY_GPIF_HEADER_H_ */

/*End of File*/
