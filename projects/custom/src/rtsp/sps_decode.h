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
    for (int i = 0; i < (tmp_buf_size - 2); ++i) {
        if ((buffer[i] ^ 0x00) + (buffer[i + 1] ^ 0x00) + (buffer[i + 2] ^ 0x03) == 0) {
            for (int j = i + 2; j < tmp_buf_size - 1; j++) {
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
        for (int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++) {
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
            uint32_t overscan_appropriate_flagu = u(1, buffer, &start_bit);
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

#ifdef __cplusplus
}
#endif
