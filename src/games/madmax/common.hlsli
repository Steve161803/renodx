#include "./shared.h"
#include "./test35.hlsli"

float3 ApplyDisplayMap(float3 untonemapped) {
  const float peak_ratio = RENODX_PEAK_WHITE_NITS / RENODX_DIFFUSE_WHITE_NITS;

  float3 tonemapped_bt709 = renodx::tonemap::psychov::psychotm_test35(
    untonemapped,                                          // bt709_linear_input
    peak_ratio,                                            // peak_value
    RENODX_TONE_MAP_EXPOSURE,                              // exposure
    RENODX_TONE_MAP_HIGHLIGHTS,                            // highlights
    RENODX_TONE_MAP_SHADOWS,                               // shadows
    max(RENODX_TONE_MAP_CONTRAST, 0.0001f),                // contrast (positive response domain)
    RENODX_TONE_MAP_SATURATION,                            // purity_scale
    1.f,                                                   // bleaching_intensity
    100.f,                                                 // clip_point
    RENODX_TONE_MAP_HUE_RESTORE - 0.5f,                    // hue_restore
    1.f,                                                   // adaptation_contrast
    0,                                                     // white_curve_mode
    RENODX_TONE_MAP_CONE_RESPONSE,                         // cone_response_exponent
    0.18f.xxx,                                             // current_adaptive_state_bt709
    0.18f.xxx,                                             // current_background_state_bt709
    1.f,                                                   // gamut_compression
    1,                                                     // gamut_compression_mode
    1.f,                                                   // adaptive_normalization
    0.f,                                                   // compression
    renodx::tonemap::psychov::PSYCHO35_SOURCE_BOUNDARY_BT709, // source_boundary
    false,                                                 // human_gamut_conditioning
    false,                                                 // gamut_fill
    0,                                                     // fit_mode
    0.f);                                                  // fit_power

  return tonemapped_bt709;
}

float3 DisplayMap(float3 hdr_color, float3 hdr_color_tm, float3 sdr_color, float2 texcoord) {
  float3 output_color;

  if (RENODX_TONE_MAP_TYPE == 0.f) {
    output_color = saturate(sdr_color);
  } else {
	  float3 inverse_color = (sdr_color / hdr_color_tm);
	  float3 lerp_color = lerp(hdr_color, inverse_color, saturate(RENODX_COLOR_GRADE_STRENGTH));
	  output_color = ApplyDisplayMap(lerp_color);
  }
  if (CUSTOM_FILM_GRAIN_STRENGTH != 0) {
    output_color = renodx::effects::ApplyFilmGrain(
        output_color,
        texcoord,
        CUSTOM_RANDOM,
        CUSTOM_FILM_GRAIN_STRENGTH * 0.03f,
        1.f);
  }
  return output_color;
}

float3 FilmGrain(float3 output_color, float2 texcoord) {
  if (CUSTOM_FILM_GRAIN_STRENGTH != 0) {
    output_color = renodx::effects::ApplyFilmGrain(
        output_color,
        texcoord,
        CUSTOM_RANDOM,
        CUSTOM_FILM_GRAIN_STRENGTH * 0.03f,
        1.f);
  }
  return output_color;
}

float3 LutSample(float3 color, Texture3D lut_texture, SamplerState lut_sample) {
  float3 lut_input_color = saturate(color);
  renodx::lut::Config lut_config = renodx::lut::config::Create();
  lut_config.lut_sampler = lut_sample;
  lut_config.strength = 1.f;
  lut_config.scaling = RENODX_COLOR_GRADE_LUT_SCALING;
  lut_config.type_input = renodx::lut::config::type::GAMMA_2_0;
  lut_config.type_output = renodx::lut::config::type::GAMMA_2_0;
  lut_config.size = 32u;
  lut_config.tetrahedral = false;
  lut_config.max_channel = 0.f;
  color = renodx::lut::Sample(lut_texture, lut_config, color);
  color = lerp(lut_input_color, color, RENODX_COLOR_GRADE_LUT_STRENGTH);
  return color;
}
