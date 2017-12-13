#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* index based on 0, right -> left (low pos for low bit) */
static int bit_at_pos_h(const uint8_t *bit_stream, uint32_t bit_pos) {
    if (!bit_stream) {
        return -1;
    }
    return ((bit_stream[bit_pos / 8] >> (bit_pos % 8)) & 0x01);
}

/* index based on 1, left -> right (low pos for high bit) */
static int bit_at_pos_n(const uint8_t *bit_stream, uint32_t bit_pos) {
    if (!bit_stream || bit_pos < 1) {
        return -1;
    }
    int32_t m;
    if ((m = bit_pos % 8) == 0) {
        return bit_stream[bit_pos / 8 - 1] & 0x01;
    } else {
        return (bit_stream[bit_pos / 8] >> (8 - m)) & 0x01;
    }
}

static uint32_t bits_at_pos_h(const uint8_t *bit_stream, uint32_t *bit_pos, uint32_t bit_count) {
    uint32_t code_num = 0;
    for (int32_t b = bit_count; b > 0; b--) {
        code_num = code_num | (bit_at_pos_h(bit_stream, (*bit_pos)++) << (b - 1));
    }
    return code_num;
}

static uint32_t bits_at_pos_n(const uint8_t *bit_stream, uint32_t *bit_pos, uint32_t bit_count) {
    uint32_t code_num = 0;
    for (int32_t b = bit_count; b > 0; b--) {
        code_num = code_num | (bit_at_pos_n(bit_stream, (*bit_pos)++) << (b - 1));
    }
    return code_num;
}

static uint32_t decode_exp_golomb_unsigned(const uint8_t *bit_stream, uint32_t *start_bit) {
    if (*start_bit <= 0) {
        LOGW("decode_exp_golomb: start_bit from 1(the highest bit of first one byte, "
                     "8 is the lowest bit of first one byte)!");
        return 0;
    }

    int32_t leading_zero_bits = -1;
    uint32_t pos = *start_bit;
    for (int32_t b = 0; !b; ++leading_zero_bits) {
        b = bit_at_pos_n(bit_stream, pos++);
    }
    uint32_t result = ((1 << leading_zero_bits) - 1
                       + bits_at_pos_n(bit_stream, &pos, (uint32_t) leading_zero_bits));
    *start_bit = pos;
    return result;
}

static int32_t decode_exp_golomb_signed(uint8_t *bit_stream, uint32_t *start_bit) {
    uint32_t ue = decode_exp_golomb_unsigned(bit_stream, start_bit);
    int32_t result = (int32_t) ceil((double) ue / 2);
    if (ue % 2 == 0) {
        return -result;
    } else {
        return result;
    }
}

static void de_emulation_prevention(uint8_t *buffer, uint32_t *buffer_size) {
    const uint32_t tmp_buf_size = *buffer_size;
    for (int i = 0; i < ((int) tmp_buf_size - 2); ++i) {
        if ((buffer[i] ^ 0x00) + (buffer[i + 1] ^ 0x00) + (buffer[i + 2] ^ 0x03) == 0) {
            for (int j = i + 2; j < ((int) tmp_buf_size - 1); j++) {
                buffer[j] = buffer[j + 1];
            }
            --(*buffer_size);
        }
    }
}

#define u(bit_count, bit_stream, bit_pos) bits_at_pos_n(bit_stream, bit_pos, bit_count)
#define ue(bit_stream, buffer_size, start_bit) decode_exp_golomb_unsigned(bit_stream, start_bit)
#define se(bit_stream, buffer_size, start_bit) decode_exp_golomb_unsigned(bit_stream, start_bit)

static bool decode_h264_sps(uint8_t *buffer, uint32_t buffer_size,
                            uint32_t *width, uint32_t *height, uint32_t *fps) {
    const uint32_t total_bit = buffer_size * 8;
    uint32_t start_bit = 1;
    *width = 0;
    *height = 0;
    *fps = 0;

    de_emulation_prevention(buffer, &buffer_size);

    uint32_t forbidden_zero_bit = u(1, buffer, &start_bit);
    uint32_t nal_ref_idc = u(2, buffer, &start_bit);
    uint32_t nal_unit_type = u(5, buffer, &start_bit);
    uint32_t profile_idc = u(8, buffer, &start_bit);
    uint32_t constraint_set0_flag = u(1, buffer, &start_bit);
    uint32_t constraint_set1_flag = u(1, buffer, &start_bit);
    uint32_t constraint_set2_flag = u(1, buffer, &start_bit);
    uint32_t constraint_set3_flag = u(1, buffer, &start_bit);
    uint32_t reserved_zero_4bits = u(4, buffer, &start_bit);
    uint32_t level_idc = u(8, buffer, &start_bit);

    if (start_bit > total_bit) {
        return false;
    }

    uint32_t seq_parameter_set_id = ue(buffer, buffer_size, &start_bit);

    if (profile_idc == 100 || profile_idc == 110 ||
        profile_idc == 122 || profile_idc == 144) {
        uint32_t chroma_format_idc = ue(buffer, buffer_size, &start_bit);
        if (chroma_format_idc == 3) {
            uint32_t residual_colour_transform_flag = u(1, buffer, &start_bit);
        }
        uint32_t bit_depth_luma_minus8 = ue(buffer, buffer_size, &start_bit);
        uint32_t bit_depth_chroma_minus8 = ue(buffer, buffer_size, &start_bit);
        uint32_t qpprime_y_zero_transform_bypass_flag = u(1, buffer, &start_bit);
        uint32_t seq_scaling_matrix_present_flag = u(1, buffer, &start_bit);

        uint32_t seq_scaling_list_present_flag[8];
        if (seq_scaling_matrix_present_flag) {
            for (int i = 0; i < 8; i++) {
                seq_scaling_list_present_flag[i] = u(1, buffer, &start_bit);
            }
        }

        if (start_bit > total_bit) {
            return false;
        }
    }

    uint32_t log2_max_frame_num_minus4 = ue(buffer, buffer_size, &start_bit);
    uint32_t pic_order_cnt_type = ue(buffer, buffer_size, &start_bit);
    if (pic_order_cnt_type == 0) {
        uint32_t log2_max_pic_order_cnt_lsb_minus4 = ue(buffer, buffer_size, &start_bit);
    } else if (pic_order_cnt_type == 1) {
        uint32_t delta_pic_order_always_zero_flag = u(1, buffer, &start_bit);
        int32_t offset_for_non_ref_pic = se(buffer, buffer_size, &start_bit);
        int32_t offset_for_top_to_bottom_field = se(buffer, buffer_size, &start_bit);
        uint32_t num_ref_frames_in_pic_order_cnt_cycle = ue(buffer, buffer_size, &start_bit);

        int32_t *offset_for_ref_frame = (int32_t *) malloc((size_t) num_ref_frames_in_pic_order_cnt_cycle);
        if (!offset_for_ref_frame) {
            return false;
        }
        for (uint32_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++) {
            offset_for_ref_frame[i] = se(buffer, buffer_size, &start_bit);
        }
        free(offset_for_ref_frame);
    }
    uint32_t num_ref_frames = ue(buffer, buffer_size, &start_bit);
    if (start_bit > total_bit) {
        return false;
    }

    uint32_t gaps_in_frame_num_value_allowed_flag = u(1, buffer, &start_bit);

    uint32_t pic_width_in_mbs_minus1 = ue(buffer, buffer_size, &start_bit);
    uint32_t pic_height_in_map_units_minus1 = ue(buffer, buffer_size, &start_bit);
    *width = (pic_width_in_mbs_minus1 + 1) * 16;
    *height = (pic_height_in_map_units_minus1 + 1) * 16;
    if (start_bit > total_bit) {
        return false;
    }

    uint32_t frame_mbs_only_flag = u(1, buffer, &start_bit);
    if (frame_mbs_only_flag == 0) {
        uint32_t mb_adaptive_frame_field_flag = u(1, buffer, &start_bit);
    }

    uint32_t direct_8x8_inference_flag = u(1, buffer, &start_bit);
    uint32_t frame_cropping_flag = u(1, buffer, &start_bit);
    if (frame_cropping_flag != 0) {
        uint32_t frame_crop_left_offset = ue(buffer, buffer_size, &start_bit);
        uint32_t frame_crop_right_offset = ue(buffer, buffer_size, &start_bit);
        uint32_t frame_crop_top_offset = ue(buffer, buffer_size, &start_bit);
        uint32_t frame_crop_bottom_offset = ue(buffer, buffer_size, &start_bit);
        if (start_bit > total_bit) {
            return false;
        }
    }
    uint32_t vui_parameter_present_flag = u(1, buffer, &start_bit);
    if (vui_parameter_present_flag != 0) {
        uint32_t aspect_ratio_info_present_flag = u(1, buffer, &start_bit);
        if (aspect_ratio_info_present_flag) {
            uint32_t aspect_ratio_idc = u(8, buffer, &start_bit);
            if (aspect_ratio_idc == 255) {
                uint32_t sar_width = u(16, buffer, &start_bit);
                uint32_t sar_height = u(16, buffer, &start_bit);
                if (start_bit > total_bit) {
                    return false;
                }
            }
        }
        uint32_t overscan_info_present_flag = u(1, buffer, &start_bit);
        if (overscan_info_present_flag != 0) {
            uint32_t overscan_appropriate_flag = u(1, buffer, &start_bit);
        }
        uint32_t video_signal_type_present_flag = u(1, buffer, &start_bit);
        if (video_signal_type_present_flag != 0) {
            uint32_t video_format = u(3, buffer, &start_bit);
            uint32_t video_full_range_flag = u(1, buffer, &start_bit);
            uint32_t colour_description_present_flag = u(1, buffer, &start_bit);
            if (colour_description_present_flag != 0) {
                uint32_t colour_primaries = u(8, buffer, &start_bit);
                uint32_t transfer_characteristics = u(8, buffer, &start_bit);
                uint32_t matrix_coefficients = u(8, buffer, &start_bit);
                if (start_bit > total_bit) {
                    return false;
                }
            }
        }
        uint32_t chroma_loc_info_present_flag = u(1, buffer, &start_bit);
        if (chroma_loc_info_present_flag != 0) {
            uint32_t chroma_sample_loc_type_top_field = ue(buffer, buffer_size, &start_bit);
            uint32_t chroma_sample_loc_type_bottom_field = ue(buffer, buffer_size, &start_bit);
        }
        uint32_t timing_info_present_flag = u(1, buffer, &start_bit);
        if (timing_info_present_flag != 0) {
            uint32_t num_units_in_tick = u(32, buffer, &start_bit);
            uint32_t time_scale = u(32, buffer, &start_bit);
            *fps = time_scale / (2 * num_units_in_tick);
        }
        if (start_bit > total_bit) {
            return false;
        }
    }
    return true;
}

static bool decode_h265_sps(uint8_t *buffer, uint32_t buffer_size,
                            uint32_t *width, uint32_t *height, uint32_t *fps) {
    const uint32_t total_bit = buffer_size * 8;
    uint32_t start_bit = 1;
    *width = 0;
    *height = 0;
    *fps = 0;

    /* nalu header */
    de_emulation_prevention(buffer, &buffer_size);

    uint32_t forbidden_zero_bit = u(1, buffer, &start_bit);
    uint32_t nal_unit_type = u(6, buffer, &start_bit);
    uint32_t nuh_reserved_zero_6bits = u(6, buffer, &start_bit);
    uint32_t nuh_temporal_id_plus1 = u(3, buffer, &start_bit);

    /* sps */
    uint32_t sps_video_parameter_set_id = u(4, buffer, &start_bit);
    uint32_t sps_max_sub_layers_minus1 = u(3, buffer, &start_bit);
    uint32_t sps_temporal_id_nesting_flag = u(1, buffer, &start_bit);

    uint32_t general_profile_space = u(2, buffer, &start_bit);
    uint32_t general_tier_flag = u(1, buffer, &start_bit);
    uint32_t general_profile_idc = u(5, buffer, &start_bit);
    uint32_t general_profile_compatibility_flag[32];
    for (int j = 0; j < 32; ++j) {
        general_profile_compatibility_flag[j] = u(1, buffer, &start_bit);
    }
    uint32_t general_progressive_source_flag = u(1, buffer, &start_bit);
    uint32_t general_interlaced_source_flag = u(1, buffer, &start_bit);
    uint32_t general_non_packed_constraint_flag = u(1, buffer, &start_bit);
    uint32_t general_frame_only_constraint_flag = u(1, buffer, &start_bit);
    if (general_profile_idc == 4 || general_profile_compatibility_flag[4]
        || general_profile_idc == 5 || general_profile_compatibility_flag[5]
        || general_profile_idc == 6 || general_profile_compatibility_flag[6]
        || general_profile_idc == 7 || general_profile_compatibility_flag[7]
        || general_profile_idc == 8 || general_profile_compatibility_flag[8]
        || general_profile_idc == 9 || general_profile_compatibility_flag[9]
        || general_profile_idc == 10 || general_profile_compatibility_flag[10]) {
        /* The number of bits in this syntax structure is not affected by this condition */
        uint32_t general_max_12bit_constraint_flag = u(1, buffer, &start_bit);
        uint32_t general_max_10bit_constraint_flag = u(1, buffer, &start_bit);
        uint32_t general_max_8bit_constraint_flag = u(1, buffer, &start_bit);
        uint32_t general_max_422chroma_constraint_flag = u(1, buffer, &start_bit);
        uint32_t general_max_420chroma_constraint_flag = u(1, buffer, &start_bit);
        uint32_t general_max_monochrome_constraint_flag = u(1, buffer, &start_bit);
        uint32_t general_intra_constraint_flag = u(1, buffer, &start_bit);
        uint32_t general_one_picture_only_constraint_flag = u(1, buffer, &start_bit);
        uint32_t general_lower_bit_rate_constraint_flag = u(1, buffer, &start_bit);
        if (general_profile_idc == 5 || general_profile_compatibility_flag[5]
           || general_profile_idc == 9 || general_profile_compatibility_flag[9]
           || general_profile_idc == 10 || general_profile_compatibility_flag[10]) {
            uint32_t general_max_14bit_constraint_flag = u(1, buffer, &start_bit);
            uint32_t general_reserved_zero_33bits_0 = u(32, buffer, &start_bit);
            uint32_t general_reserved_zero_33bits_1 = u(1, buffer, &start_bit);
        } else {
            uint32_t general_reserved_zero_34bits_0 = u(32, buffer, &start_bit);
            uint32_t general_reserved_zero_34bits_1 = u(2, buffer, &start_bit);
        }
    } else {
        uint32_t general_reserved_zero_43bits_0 = u(32, buffer, &start_bit);
        uint32_t general_reserved_zero_43bits_1 = u(11, buffer, &start_bit);
    }

    if ((general_profile_idc >= 1 && general_profile_idc <= 5)
       || general_profile_idc == 9 || general_profile_compatibility_flag[1]
       || general_profile_compatibility_flag[2] || general_profile_compatibility_flag[3]
       || general_profile_compatibility_flag[4] || general_profile_compatibility_flag[5]
       || general_profile_compatibility_flag[9]) {
        /* The number of bits in this syntax structure is not affected by this condition */
        uint32_t general_inbld_flag = u(1, buffer, &start_bit);
    } else {
        uint32_t general_reserved_zero_bit = u(1, buffer, &start_bit);
    }

    uint32_t general_level_idc = u(8, buffer, &start_bit);
    uint32_t sub_layer_profile_present_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_level_present_flag[sps_max_sub_layers_minus1];
    for (uint32_t i = 0; i < sps_max_sub_layers_minus1; ++i) {
        sub_layer_profile_present_flag[i] = u(1, buffer, &start_bit);
        sub_layer_level_present_flag[i] = u(1, buffer, &start_bit);
    }
    if (sps_max_sub_layers_minus1 > 0) {
        uint32_t reserved_zero_2bits[8 - sps_max_sub_layers_minus1];
        for (uint32_t i = sps_max_sub_layers_minus1; i < 8; ++i) {
            reserved_zero_2bits[i] = u(2, buffer, &start_bit);
        }
    }
    uint32_t sub_layer_profile_space[sps_max_sub_layers_minus1];
    uint32_t sub_layer_tier_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_profile_idc[sps_max_sub_layers_minus1];
    uint32_t sub_layer_progressive_source_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_interlaced_source_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_non_packed_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_frame_only_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_max_12bit_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_max_10bit_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_max_8bit_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_max_422chroma_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_max_420chroma_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_max_monochrome_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_intra_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_one_picture_only_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_lower_bit_rate_constraint_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_inbld_flag[sps_max_sub_layers_minus1];
    uint32_t sub_layer_level_idc[sps_max_sub_layers_minus1];

    for (uint32_t i = 0; i < sps_max_sub_layers_minus1; ++i) {
        if (sub_layer_profile_present_flag[i]) {
            sub_layer_profile_space[i] = u(2, buffer, &start_bit);
            sub_layer_tier_flag[i] = u(1, buffer, &start_bit);
            sub_layer_profile_idc[i] = u(5, buffer, &start_bit);
            uint32_t sub_layer_profile_compatibility_flag[sps_max_sub_layers_minus1][32];
            for (uint32_t j = 0; j < 32; ++j) {
                sub_layer_profile_compatibility_flag[i][j] = u(1, buffer, &start_bit);
            }
            sub_layer_progressive_source_flag[i] = u(1, buffer, &start_bit);
            sub_layer_interlaced_source_flag[i] = u(1, buffer, &start_bit);
            sub_layer_non_packed_constraint_flag[i] = u(1, buffer, &start_bit);
            sub_layer_frame_only_constraint_flag[i] = u(1, buffer, &start_bit);
            if (sub_layer_profile_idc[i] == 4 || sub_layer_profile_compatibility_flag[i][4]
                || sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[i][5]
                || sub_layer_profile_idc[i] == 6 || sub_layer_profile_compatibility_flag[i][6]
                || sub_layer_profile_idc[i] == 7 || sub_layer_profile_compatibility_flag[i][7]
                || sub_layer_profile_idc[i] == 8 || sub_layer_profile_compatibility_flag[i][8]
                || sub_layer_profile_idc[i] == 9 || sub_layer_profile_compatibility_flag[i][9]
                || sub_layer_profile_idc[i] == 10 || sub_layer_profile_compatibility_flag[i][10]) {
                /* The number of bits in this syntax structure is not affected by this condition */
                sub_layer_max_12bit_constraint_flag[i] = u(1, buffer, &start_bit);
                sub_layer_max_10bit_constraint_flag[i] = u(1, buffer, &start_bit);
                sub_layer_max_8bit_constraint_flag[i] = u(1, buffer, &start_bit);
                sub_layer_max_422chroma_constraint_flag[i] = u(1, buffer, &start_bit);
                sub_layer_max_420chroma_constraint_flag[i] = u(1, buffer, &start_bit);
                sub_layer_max_monochrome_constraint_flag[i] = u(1, buffer, &start_bit);
                sub_layer_intra_constraint_flag[i] = u(1, buffer, &start_bit);
                sub_layer_one_picture_only_constraint_flag[i] = u(1, buffer, &start_bit);
                sub_layer_lower_bit_rate_constraint_flag[i] = u(1, buffer, &start_bit);
                if (sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[5] ) {
                    uint32_t sub_layer_max_14bit_constraint_flag = u(1, buffer, &start_bit);
                    /* sub_layer_reserved_zero_33bits */
                    u(32, buffer, &start_bit);
                    u(1, buffer, &start_bit);
                } else {
                    /* sub_layer_reserved_zero_34bits */
                    u(32, buffer, &start_bit);
                    u(2, buffer, &start_bit);
                }
            } else {
                /* sub_layer_reserved_zero_43bits */
                u(32, buffer, &start_bit);
                u(11, buffer, &start_bit);
            }
            if ((sub_layer_profile_idc[i] >= 1 && sub_layer_profile_idc[i] <= 5)
               || sub_layer_profile_idc[i] == 9 || sub_layer_profile_compatibility_flag[1]
               || sub_layer_profile_compatibility_flag[2] || sub_layer_profile_compatibility_flag[3]
               || sub_layer_profile_compatibility_flag[4] || sub_layer_profile_compatibility_flag[5]
               || sub_layer_profile_compatibility_flag[9]) {
                /* The number of bits in this syntax structure is not affected by this condition */
                sub_layer_inbld_flag[i] = u(1, buffer, &start_bit);
            } else {
                /* sub_layer_reserved_zero_bits */
                u(1, buffer, &start_bit);
            }
        }
        if (sub_layer_level_present_flag[i]) {
            sub_layer_level_idc[i] = u(8, buffer, &start_bit);
        }
    }

    uint32_t sps_seq_parameter_set_id = ue(buffer, buffer_size, &start_bit);
    uint32_t chroma_format_idc = ue(buffer, buffer_size, &start_bit);
    if (chroma_format_idc == 3) {
        uint32_t separate_colour_plane_flag = u(1, buffer, &start_bit);
    }
    *width  = ue(buffer, buffer_size, &start_bit); /* pic_width_in_luma_samples */
    *height  = ue(buffer, buffer_size, &start_bit); /* pic_height_in_luma_samples */
    uint32_t conformance_window_flag = u(1, buffer, &start_bit);
    if (conformance_window_flag) {
        uint32_t conf_win_left_offset = ue(buffer, buffer_size, &start_bit);
        uint32_t conf_win_right_offset = ue(buffer, buffer_size, &start_bit);
        uint32_t conf_win_top_offset = ue(buffer, buffer_size, &start_bit);
        uint32_t conf_win_bottom_offset = ue(buffer, buffer_size, &start_bit);
    }
    uint32_t bit_depth_luma_minus8 = ue(buffer, buffer_size, &start_bit);
    uint32_t bit_depth_chroma_minus8 = ue(buffer, buffer_size, &start_bit);
    uint32_t log2_max_pic_order_cnt_lsb_minus4 = ue(buffer, buffer_size, &start_bit);
    uint32_t sps_sub_layer_ordering_info_present_flag = u(1, buffer, &start_bit);

    uint32_t sps_max_dec_pic_buffering_minus1[sps_max_sub_layers_minus1 + 1];
    uint32_t sps_max_num_reorder_pics[sps_max_sub_layers_minus1 + 1];
    uint32_t sps_max_latency_increase_plus1[sps_max_sub_layers_minus1 + 1];
    for (uint32_t i = (sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1);
         i <= sps_max_sub_layers_minus1;
         ++i) {
        sps_max_dec_pic_buffering_minus1[i] = ue(buffer, buffer_size, &start_bit);
        sps_max_num_reorder_pics[i] = ue(buffer, buffer_size, &start_bit);
        sps_max_latency_increase_plus1[i] = ue(buffer, buffer_size, &start_bit);
    }

    uint32_t log2_min_luma_coding_block_size_minus3 = ue(buffer, buffer_size, &start_bit);
    uint32_t log2_diff_max_min_luma_coding_block_size = ue(buffer, buffer_size, &start_bit);
    uint32_t log2_min_luma_transform_block_size_minus2 = ue(buffer, buffer_size, &start_bit);
    uint32_t log2_diff_max_min_luma_transform_block_size = ue(buffer, buffer_size, &start_bit);
    uint32_t max_transform_hierarchy_depth_inter = ue(buffer, buffer_size, &start_bit);
    uint32_t max_transform_hierarchy_depth_intra = ue(buffer, buffer_size, &start_bit);
    uint32_t scaling_list_enabled_flag = u(1, buffer, &start_bit);
    if (scaling_list_enabled_flag) {
        uint32_t sps_scaling_list_data_present_flag = u(1, buffer, &start_bit);
        if (sps_scaling_list_data_present_flag) {
            /* scaling_list_data(); */
        }
    }

    uint32_t amp_enabled_flag = u(1, buffer, &start_bit);
    uint32_t sample_adaptive_offset_enabled_flag = u(1, buffer, &start_bit);
    uint32_t pcm_enabled_flag = u(1, buffer, &start_bit);
    if (pcm_enabled_flag) {
        uint32_t pcm_sample_bit_depth_luma_minus1 = u(4, buffer, &start_bit);
        uint32_t pcm_sample_bit_depth_chroma_minus1 = u(4, buffer, &start_bit);
        uint32_t log2_min_pcm_luma_coding_block_size_minus3 = ue(buffer, buffer_size, &start_bit);
        uint32_t log2_diff_max_min_pcm_luma_coding_block_size = ue(buffer, buffer_size, &start_bit);
        uint32_t pcm_loop_filter_disabled_flag = u(1, buffer, &start_bit);
    }
    uint32_t num_short_term_ref_pic_sets = ue(buffer, buffer_size, &start_bit);
    for (uint32_t i = 0; i < num_short_term_ref_pic_sets; ++i) {
        /* st_ref_pic_set(i); */
    }

    uint32_t long_term_ref_pics_present_flag = u(1, buffer, &start_bit);
    if (long_term_ref_pics_present_flag) {
        uint32_t num_long_term_ref_pics_sps = ue(buffer, buffer_size, &start_bit);
        uint32_t lt_ref_pic_poc_lsb_sps[num_long_term_ref_pics_sps];
        uint32_t used_by_curr_pic_lt_sps_flag[num_long_term_ref_pics_sps];
        for (uint32_t i = 0; i < num_long_term_ref_pics_sps; ++i) {
            lt_ref_pic_poc_lsb_sps[i] = ue(buffer, buffer_size, &start_bit);
            used_by_curr_pic_lt_sps_flag[i] = u(1, buffer, &start_bit);
        }
    }

    uint32_t sps_temporal_mvp_enabled_flag = u(1, buffer, &start_bit);
    uint32_t strong_intra_smoothing_enabled_flag = u(1, buffer, &start_bit);
    uint32_t vui_parameters_present_flag = u(1, buffer, &start_bit);
    if (vui_parameters_present_flag) {
        uint32_t aspect_ratio_info_present_flag = u(1, buffer, &start_bit);
        if (aspect_ratio_info_present_flag) {
            uint32_t aspect_ratio_idc = u(8, buffer, &start_bit);
            if (aspect_ratio_idc == 255) {
                uint32_t sar_width = u(16, buffer, &start_bit);
                uint32_t sar_height = u(16, buffer, &start_bit);
            }
        }
        uint32_t overscan_info_present_flag = u(1, buffer, &start_bit);
        if (overscan_info_present_flag) {
            uint32_t overscan_appropriate_flag = u(1, buffer, &start_bit);
        }
        uint32_t video_signal_type_present_flag = u(1, buffer, &start_bit);
        if (video_signal_type_present_flag) {
            uint32_t video_format = u(3, buffer, &start_bit);
            uint32_t video_full_range_flag = u(1, buffer, &start_bit);
            uint32_t colour_description_present_flag = u(1, buffer, &start_bit);
            if (colour_description_present_flag) {
                uint32_t colour_primaries = u(8, buffer, &start_bit);
                uint32_t transfer_characteristics = u(8, buffer, &start_bit);
                uint32_t matrix_coefficients = u(8, buffer, &start_bit);
            }
        }
        uint32_t chroma_loc_info_present_flag = u(1, buffer, &start_bit);
        if (chroma_loc_info_present_flag) {
            uint32_t chroma_sample_loc_type_top_field = ue(buffer, buffer_size, &start_bit);
            uint32_t chroma_sample_loc_type_bottom_field = ue(buffer, buffer_size, &start_bit);
        }
        uint32_t neutral_chroma_indication_flag = u(1, buffer, &start_bit);
        uint32_t field_seq_flag = u(1, buffer, &start_bit);
        uint32_t frame_field_info_present_flag = u(1, buffer, &start_bit);
        uint32_t default_display_window_flag = u(1, buffer, &start_bit);
        if (default_display_window_flag) {
            uint32_t def_disp_win_left_offset = ue(buffer, buffer_size, &start_bit);
            uint32_t def_disp_win_right_offset = ue(buffer, buffer_size, &start_bit);
            uint32_t def_disp_win_top_offset = ue(buffer, buffer_size, &start_bit);
            uint32_t def_disp_win_bottom_offset = ue(buffer, buffer_size, &start_bit);
        }
        uint32_t vui_timing_info_present_flag = u(1, buffer, &start_bit);
        if (vui_timing_info_present_flag) {
            uint32_t vui_num_units_in_tick = u(32, buffer, &start_bit);
            uint32_t vui_time_scale = u(32, buffer, &start_bit);
            uint32_t vui_poc_proportional_to_timing_flag = u(1, buffer, &start_bit);
            if (vui_poc_proportional_to_timing_flag) {
                uint32_t vui_num_ticks_poc_diff_one_minus1 = ue(buffer, buffer_size, &start_bit);
            }
            uint32_t vui_hrd_parameters_present_flag = u(1, buffer, &start_bit);
            if (vui_hrd_parameters_present_flag) {
                /* hrd_parameters(1, sps_max_sub_layers_minus1); */
            }
            *fps = vui_time_scale / (2 * vui_num_units_in_tick);
        }
        uint32_t bitstream_restriction_flag = u(1, buffer, &start_bit);
        if (bitstream_restriction_flag) {
            uint32_t tiles_fixed_structure_flag = u(1, buffer, &start_bit);
            uint32_t motion_vectors_over_pic_boundaries_flag = u(1, buffer, &start_bit);
            uint32_t restricted_ref_pic_lists_flag = u(1, buffer, &start_bit);
            uint32_t min_spatial_segmentation_idc = ue(buffer, buffer_size, &start_bit);
            uint32_t max_bytes_per_pic_denom = ue(buffer, buffer_size, &start_bit);
            uint32_t max_bits_per_min_cu_denom = ue(buffer, buffer_size, &start_bit);
            uint32_t log2_max_mv_length_horizontal = ue(buffer, buffer_size, &start_bit);
            uint32_t log2_max_mv_length_vertical = ue(buffer, buffer_size, &start_bit);
        }
    }

    uint32_t sps_extension_present_flag = u(1, buffer, &start_bit);
    if (sps_extension_present_flag) {
        uint32_t sps_range_extension_flag = u(1, buffer, &start_bit);
        uint32_t sps_multilayer_extension_flag = u(1, buffer, &start_bit);
        uint32_t sps_3d_extension_flag = u(1, buffer, &start_bit);
        uint32_t sps_scc_extension_flag = u(1, buffer, &start_bit);
        uint32_t sps_extension_4bits = u(4, buffer, &start_bit);
        if (sps_range_extension_flag) {
            /* sps_range_extension(); */
        }
        if (sps_multilayer_extension_flag) {
            /* sps_multilayer_extension(); */
        }
        if (sps_3d_extension_flag) {
            /* sps_3d_extension(); */
        }
        if (sps_scc_extension_flag) {
            /* sps_scc_extension(); */
        }
        if (sps_extension_4bits) {
            uint32_t sps_extension_data_flag = u(1, buffer, &start_bit);
        }
    }
    return true;
}

#ifdef __cplusplus
}
#endif
