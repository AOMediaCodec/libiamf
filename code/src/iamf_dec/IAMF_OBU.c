/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file IAMF_OBU.c
 * @brief OBU Parser.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "IAMF_OBU.h"

#include "IAMF_debug.h"
#include "IAMF_defines.h"
#include "IAMF_utils.h"
#include "bitstream.h"
#include "fixedp11_5.h"

#define SR 0
#if SR
#include "vlogging_tool_sr.h"
#endif

#define IAMF_OBU_MIN_SIZE 2

#ifdef IA_TAG
#undef IA_TAG
#endif
#define IA_TAG "IAMF_OBU"

static uint32_t iamf_obu_get_payload_size(IAMF_OBU *obu);
static IAMF_Version *iamf_version_new(IAMF_OBU *obu);

static IAMF_CodecConf *iamf_codec_conf_new(IAMF_OBU *obu);
static void iamf_codec_conf_free(IAMF_CodecConf *obj);

static IAMF_Element *iamf_element_new(IAMF_OBU *obu);
static void iamf_element_free(IAMF_Element *obj);

static IAMF_MixPresentation *iamf_mix_presentation_new(IAMF_OBU *obu);
static void iamf_mix_presentation_free(IAMF_MixPresentation *obj);

static IAMF_Parameter *iamf_parameter_new(IAMF_OBU *obu,
                                          IAMF_ParameterParam *param);
static void iamf_parameter_free(IAMF_Parameter *obj);

static IAMF_Frame *iamf_frame_new(IAMF_OBU *obu);

static IAMF_Sync *iamf_sync_new(IAMF_OBU *obu);
static void iamf_sync_free(IAMF_Sync *obj);

static void iamf_parameter_recon_gain_segment_free(ReconGainSegment *seg);

uint32_t IAMF_OBU_split(const uint8_t *data, uint32_t size, IAMF_OBU *obu) {
  BitStream b;
  uint64_t ret = 0;

  if (size < IAMF_OBU_MIN_SIZE) {
    return 0;
  }

  bs(&b, data, size);

  memset(obu, 0, sizeof(IAMF_OBU));
  obu->type = bs_get32b(&b, 5);
  obu->redundant = bs_get32b(&b, 1);
  obu->trimming = bs_get32b(&b, 1);
  obu->extension = bs_get32b(&b, 1);

  ret = bs_getAleb128(&b);

  if (ret == UINT64_MAX || ret + bs_tell(&b) > size) return 0;

  ia_logt("===============================================");
  ia_logt(
      "obu header : %s (%d) type, redundant %d, trimming %d, extension %d, "
      "payload size %lu, obu size %lu vs size %u",
      IAMF_OBU_type_string(obu->type), obu->type, obu->redundant, obu->trimming,
      obu->extension, ret, bs_tell(&b) + ret, size);

  if (obu->redundant) {
    ia_logd("%s OBU redundant.", IAMF_OBU_type_string(obu->type));
  }

  obu->data = (uint8_t *)data;
  obu->size = bs_tell(&b) + (uint32_t)ret;
  obu_dump(data, obu->size, obu->type);

  if (obu->trimming) {
    obu->trim_end = bs_getAleb128(&b);    // num_samples_to_trim_at_end;
    obu->trim_start = bs_getAleb128(&b);  // num_samples_to_trim_at_start;
    ia_logt("trim samples at start %lu, at end %lu", obu->trim_start,
            obu->trim_end);
  }

  if (obu->extension) {
    obu->ext_size = bs_getAleb128(&b);  // extension_header_size;
    obu->ext_header = (uint8_t *)obu->data + bs_tell(&b);
    ia_logt("obu extension header at %u, size %lu", bs_tell(&b), obu->ext_size);
    bs_skip(&b, obu->ext_size);  // skip extension header
  }

  ia_logt("obu payload start at %u", bs_tell(&b));
  obu->payload = (uint8_t *)data + bs_tell(&b);

#if SR
  if (obu->type == IAMF_OBU_TEMPORAL_DELIMITER)
    vlog_obu(IAMF_OBU_TEMPORAL_DELIMITER, obu, 0, 0);
#endif

  return obu->size;
}

int IAMF_OBU_is_descrptor_OBU(IAMF_OBU *obu) {
  IAMF_OBU_Type type = obu->type;

  if (type == IAMF_OBU_CODEC_CONFIG || type == IAMF_OBU_AUDIO_ELEMENT ||
      type == IAMF_OBU_MIX_PRESENTATION || type == IAMF_OBU_MAGIC_CODE) {
    return 1;
  }
  return 0;
}

uint64_t IAMF_OBU_get_object_id(IAMF_OBU *obu) {
  if (obu->type == IAMF_OBU_PARAMETER_BLOCK) {
    BitStream b;
    bs(&b, obu->payload, iamf_obu_get_payload_size(obu));
    return bs_getAleb128(&b);
  }
  return (uint64_t)-1;
}

const char *IAMF_OBU_type_string(IAMF_OBU_Type type) {
  static const char *obu_type_string[] = {
      "Codec Config",     "Audio Element",      "Mix Presentation",
      "Parameter Block",  "Temporal Delimiter", "Sync",
      "Reserved",         "Reserved",           "Audio Frame",
      "Audio Frame ID0",  "Audio Frame ID1",    "Audio Frame ID2",
      "Audio Frame ID3",  "Audio Frame ID4",    "Audio Frame ID5",
      "Audio Frame ID6",  "Audio Frame ID7",    "Audio Frame ID8",
      "Audio Frame ID9",  "Audio Frame ID10",   "Audio Frame ID11",
      "Audio Frame ID12", "Audio Frame ID13",   "Audio Frame ID14",
      "Audio Frame ID15", "Audio Frame ID16",   "Audio Frame ID17",
      "Audio Frame ID18", "Audio Frame ID19",   "Audio Frame ID20",
      "Audio Frame ID21", "Magic Code"};

  if (type < IAMF_OBU_CODEC_CONFIG || type > IAMF_OBU_MAGIC_CODE) {
    return "Invalid OBU type.";
  }
  return obu_type_string[type];
}

IAMF_Object *IAMF_object_new(IAMF_OBU *obu, IAMF_ObjectParameter *param) {
  IAMF_Object *obj = 0;

  switch (obu->type) {
    case IAMF_OBU_MAGIC_CODE:
      obj = IAMF_OBJ(iamf_version_new(obu));
      break;
    case IAMF_OBU_CODEC_CONFIG:
      obj = IAMF_OBJ(iamf_codec_conf_new(obu));
      break;
    case IAMF_OBU_AUDIO_ELEMENT:
      obj = IAMF_OBJ(iamf_element_new(obu));
      break;
    case IAMF_OBU_MIX_PRESENTATION:
      obj = IAMF_OBJ(iamf_mix_presentation_new(obu));
      break;
    case IAMF_OBU_PARAMETER_BLOCK: {
      IAMF_ParameterParam *p = IAMF_PARAMETER_PARAM(param);
      obj = IAMF_OBJ(iamf_parameter_new(obu, p));
    } break;
    case IAMF_OBU_SYNC:
      obj = IAMF_OBJ(iamf_sync_new(obu));
      break;
    default:
      if (obu->type >= IAMF_OBU_AUDIO_FRAME &&
          obu->type < IAMF_OBU_MAGIC_CODE) {
        obj = IAMF_OBJ(iamf_frame_new(obu));
      }
      break;
  }

  if (obj) {
    if (obu->redundant) obj->flags |= IAMF_OBU_FLAG_REDUNDANT;
  }

  return obj;
}

void IAMF_object_free(IAMF_Object *obj) {
  if (obj) {
    switch (obj->type) {
      case IAMF_OBU_MAGIC_CODE:
        free(obj);
        break;
      case IAMF_OBU_CODEC_CONFIG:
        iamf_codec_conf_free((IAMF_CodecConf *)obj);
        break;
      case IAMF_OBU_AUDIO_ELEMENT:
        iamf_element_free((IAMF_Element *)obj);
        break;
      case IAMF_OBU_MIX_PRESENTATION:
        iamf_mix_presentation_free((IAMF_MixPresentation *)obj);
        break;
      case IAMF_OBU_PARAMETER_BLOCK:
        iamf_parameter_free((IAMF_Parameter *)obj);
        break;
      case IAMF_OBU_SYNC:
        iamf_sync_free((IAMF_Sync *)obj);
        break;
      default:
        if (obj->type >= IAMF_OBU_AUDIO_FRAME &&
            obj->type < IAMF_OBU_MAGIC_CODE) {
          free(obj);
        }
        break;
    }
  }
}

void IAMF_parameter_segment_free(ParameterSegment *seg) {
  if (seg) {
    if (seg->type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      iamf_parameter_recon_gain_segment_free((ReconGainSegment *)seg);
    } else
      free(seg);
  }
}

uint32_t iamf_obu_get_payload_size(IAMF_OBU *obu) {
  return obu->size - (uint32_t)(obu->payload - obu->data);
}

IAMF_Version *iamf_version_new(IAMF_OBU *obu) {
  IAMF_Version *ver = 0;
  BitStream b;

  ver = IAMF_MALLOCZ(IAMF_Version, 1);
  if (!ver) {
    ia_loge("fail to allocate memory for Version Object.");
    return ver;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  ver->obj.type = IAMF_OBU_MAGIC_CODE;
  bs_read(&b, (uint8_t *)&ver->iamf_code, 4);
  ver->version = bs_getA8b(&b);
  ver->profile_version = bs_getA8b(&b);

  ia_logd(
      "magic code object: %.4s, version 0x%x(%d.%d), profile version "
      "0x%x(%d.%d)",
      (char *)&ver->iamf_code, ver->version, ver->version_major,
      ver->version_minor, ver->profile_version, ver->profile_major,
      ver->profile_minor);

#if SR
  vlog_obu(IAMF_OBU_MAGIC_CODE, ver, 0, 0);
#endif

  return ver;
}

IAMF_CodecConf *iamf_codec_conf_new(IAMF_OBU *obu) {
  IAMF_CodecConf *conf = 0;
  BitStream b;

  conf = IAMF_MALLOCZ(IAMF_CodecConf, 1);
  if (!conf) {
    ia_loge("fail to allocate memory for Codec Config Object.");
    goto codec_conf_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  conf->obj.type = IAMF_OBU_CODEC_CONFIG;

  conf->codec_conf_id = bs_getAleb128(&b);
  bs_read(&b, (uint8_t *)&conf->codec_id, 4);
  conf->nb_samples_per_frame = bs_getAleb128(&b);
  conf->roll_distance = bs_getA16b(&b);

  conf->decoder_conf_size = iamf_obu_get_payload_size(obu) - bs_tell(&b);
  conf->decoder_conf = IAMF_MALLOC(uint8_t, conf->decoder_conf_size);
  if (!conf->decoder_conf) {
    ia_loge(
        "fail to allocate memory for decoder config of Codec Config Object.");
    goto codec_conf_fail;
  }
  bs_read(&b, conf->decoder_conf, conf->decoder_conf_size);
  ia_logd(
      "codec configure object: id %lu, codec %.4s, decoder configure size %d, "
      "samples per frame %lu, roll distance %d",
      conf->codec_conf_id, (char *)&conf->codec_id, conf->decoder_conf_size,
      conf->nb_samples_per_frame, conf->roll_distance);

#if SR
  vlog_obu(IAMF_OBU_CODEC_CONFIG, conf, 0, 0);
#endif
  return conf;

codec_conf_fail:
  if (conf) {
    iamf_codec_conf_free(conf);
  }
  return 0;
}

void iamf_codec_conf_free(IAMF_CodecConf *obj) {
  IAMF_FREE(obj->decoder_conf);
  free(obj);
}

static int iamf_parameter_base_init(ParameterBase *pb, IAMF_ParameterType type,
                                    BitStream *b) {
  pb->type = type;
  pb->id = bs_getAleb128(b);
  pb->rate = bs_getAleb128(b);
  pb->mode = bs_get32b(b, 1);
  ia_logd("Parameter Base: type %lu, id %lu, rate %lu, mode %u", pb->type,
          pb->id, pb->rate, pb->mode);
  if (!pb->mode) {
    pb->duration = bs_getAleb128(b);
    pb->nb_segments = bs_getAleb128(b);
    pb->constant_segment_interval = bs_getAleb128(b);
    ia_logd(
        "\tduration %lu, number of segment %lu, constant segment interval %lu",
        pb->duration, pb->nb_segments, pb->constant_segment_interval);
    if (!pb->constant_segment_interval) {
      pb->segments = IAMF_MALLOCZ(ParameterSegment, pb->nb_segments);
      if (!pb->segments) return IAMF_ERR_ALLOC_FAIL;
      for (int i = 0; i < pb->nb_segments; ++i) {
        ia_logd("\tSegment %d: %lu", i, pb->segments[i].segment_interval);
        pb->segments[i].segment_interval = bs_getAleb128(b);
      }
    }
  }

  return IAMF_OK;
}

IAMF_Element *iamf_element_new(IAMF_OBU *obu) {
  IAMF_Element *elem = 0;
  BitStream b;
  uint32_t val;
  uint64_t type;
  ParameterBase *p = 0;

  elem = IAMF_MALLOCZ(IAMF_Element, 1);
  if (!elem) {
    ia_loge("fail to allocate memory for Audio Element Object.");
    goto element_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  elem->obj.type = IAMF_OBU_AUDIO_ELEMENT;
  elem->element_id = bs_getAleb128(&b);
  elem->element_type = bs_get32b(&b, 3);
  bs_get32b(&b, 5);

  elem->codec_config_id = bs_getAleb128(&b);

  val = bs_getAleb128(&b);
  elem->nb_substreams = val;
  ia_logd("element id %lu, type %d, codec config id %lu, sub-streams count %lu",
          elem->element_id, elem->element_type, elem->codec_config_id,
          elem->nb_substreams);
  elem->substream_ids = IAMF_MALLOC(uint64_t, val);
  if (!elem->substream_ids) {
    ia_loge(
        "fail to allocate memory for substream ids of Audio Element Object.");
    goto element_fail;
  }
  for (uint32_t i = 0; i < val; ++i) {
    elem->substream_ids[i] = bs_getAleb128(&b);
    ia_logd("\t > sub-stream id %lu", elem->substream_ids[i]);
  }

  val = bs_getAleb128(&b);
  elem->nb_parameters = val;
  if (val) {
    elem->parameters = IAMF_MALLOCZ(ParameterBase *, val);
    if (!elem->parameters) {
      ia_loge(
          "fail to allocate memory for parameters of Audio Element Object.");
      goto element_fail;
    }
  }
  ia_logd("element parameters count %lu", elem->nb_parameters);
  for (uint32_t i = 0; i < val; ++i) {
    type = bs_getAleb128(&b);
    p = 0;
    if (type == IAMF_PARAMETER_TYPE_DEMIXING) {
      DemixingParameter *dp = IAMF_MALLOCZ(DemixingParameter, 1);
      p = PARAMETER_BASE(dp);
    } else if (type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      ReconGainParameter *rgp = IAMF_MALLOCZ(ReconGainParameter, 1);
      p = PARAMETER_BASE(rgp);
    } else {
      ia_loge("Invalid parameter type of Audio Element Object.");
      goto element_fail;
    }

    if (!p) {
      ia_loge(
          "fail to allocate memory for parameter object of Audio Element "
          "Object.");
      goto element_fail;
    }
    elem->parameters[i] = p;
    if (iamf_parameter_base_init(p, type, &b) != IAMF_OK) goto element_fail;
  }

  if (elem->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED) {
    ScalableChannelLayoutConf *chs_conf;
    chs_conf = IAMF_MALLOCZ(ScalableChannelLayoutConf, 1);
    if (!chs_conf) {
      ia_loge(
          "fail to allocate memory for scalable channel config of Audio "
          "Element Object.");
      goto element_fail;
    }
    elem->channels_conf = chs_conf;

    val = bs_get32b(&b, 3);
    bs_skip(&b, 5);
    chs_conf->nb_layers = val;
    ia_logd("scalable channel layers %d", chs_conf->nb_layers);
    if (val) {
      ChannelLayerConf *layer_conf_s;
      layer_conf_s = IAMF_MALLOCZ(ChannelLayerConf, val);
      if (!layer_conf_s) {
        ia_loge(
            "fail to allocate memory for channel layer config of Audio Element "
            "Object.");
        goto element_fail;
      }
      chs_conf->layer_conf_s = layer_conf_s;
      for (uint32_t i = 0; i < val; ++i) {
        layer_conf_s[i].loudspeaker_layout = bs_get32b(&b, 4);
        layer_conf_s[i].output_gain_flag = bs_get32b(&b, 1);
        layer_conf_s[i].recon_gain_flag = bs_get32b(&b, 1);
        layer_conf_s[i].nb_substreams = bs_getA8b(&b);
        layer_conf_s[i].nb_coupled_substreams = bs_getA8b(&b);
        ia_logd(
            "\tlayer[%d] info: layout %d, output gain %d, recon gain %d, "
            "sub-streams count %d, coupled sub-streams %d",
            i, layer_conf_s[i].loudspeaker_layout,
            layer_conf_s[i].output_gain_flag, layer_conf_s[i].recon_gain_flag,
            layer_conf_s[i].nb_substreams,
            layer_conf_s[i].nb_coupled_substreams);

        if (layer_conf_s[i].output_gain_flag) {
          OutputGain *g = IAMF_MALLOCZ(OutputGain, 1);
          if (!g) {
            ia_loge(
                "fail to allocate memory for out gain of Audio Element "
                "Object.");
            goto element_fail;
          }
          layer_conf_s[i].output_gain_info = g;
          g->output_gain_flag = bs_get32b(&b, 6);
          g->output_gain = (int16_t)bs_getA16b(&b);
          ia_logd("\toutput gain : flag 0x%x, gain 0x%x",
                  g->output_gain_flag & U8_MASK, g->output_gain & U16_MASK);
        }
      }
    }
  } else if (elem->element_type == AUDIO_ELEMENT_TYPE_SCENE_BASED) {
    AmbisonicsConf *conf = IAMF_MALLOCZ(AmbisonicsConf, 1);
    if (!conf) {
      ia_loge(
          "fail to allocate memory for ambisonics config of Audio Element "
          "Object.");
      goto element_fail;
    }

    elem->ambisonics_conf = conf;
    conf->ambisonics_mode = bs_getAleb128(&b);
    if (conf->ambisonics_mode == AMBISONICS_MODE_MONO) {
      conf->output_channel_count = bs_getA8b(&b);
      conf->substream_count = bs_getA8b(&b);
      conf->mapping_size = conf->output_channel_count;
      conf->mapping = IAMF_MALLOCZ(uint8_t, conf->mapping_size);
      if (!conf->mapping) {
        ia_loge(
            "fail to allocate memory for mono mapping of Audio Element "
            "Object.");
        goto element_fail;
      }
      bs_read(&b, conf->mapping, conf->mapping_size);
      ia_logd(
          "Ambisonics mode mono, channels %d, sub-stream %d, mapping size %d",
          conf->output_channel_count, conf->substream_count,
          conf->mapping_size);
    } else if (conf->ambisonics_mode == AMBISONICS_MODE_PROJECTION) {
      conf->output_channel_count = bs_getA8b(&b);
      conf->substream_count = bs_getA8b(&b);
      conf->coupled_substream_count = bs_getA8b(&b);
      conf->mapping_size =
          2 * conf->output_channel_count *
          (conf->substream_count + conf->coupled_substream_count);
      conf->mapping = IAMF_MALLOCZ(uint8_t, conf->mapping_size);
      if (!conf->mapping) {
        ia_loge(
            "fail to allocate memory for projection mapping of Audio Element "
            "Object.");
        goto element_fail;
      }
      bs_read(&b, conf->mapping, conf->mapping_size);
      ia_logd(
          "Ambisonics mode projection, channels %d, sub-stream %d, coupled "
          "sub-stream %d, matrix (%d x %d) size %d ",
          conf->output_channel_count, conf->substream_count,
          conf->coupled_substream_count, conf->output_channel_count,
          conf->substream_count + conf->coupled_substream_count,
          conf->mapping_size);
    }
  }

#if SR
  vlog_obu(IAMF_OBU_AUDIO_ELEMENT, elem, 0, 0);
#endif
  return elem;

element_fail:
  if (elem) {
    iamf_element_free(elem);
  }
  return 0;
}

void iamf_element_free(IAMF_Element *obj) {
  IAMF_FREE(obj->substream_ids);

  if (obj->parameters) {
    for (int i = 0; i < obj->nb_parameters; ++i) {
      if (obj->parameters[i]) {
        IAMF_FREE(obj->parameters[i]->segments);
        free(obj->parameters[i]);
      }
    }
    free(obj->parameters);
  }

  if (obj->element_type == AUDIO_ELEMENT_TYPE_CHANNEL_BASED &&
      obj->channels_conf) {
    ScalableChannelLayoutConf *conf = obj->channels_conf;
    if (conf->layer_conf_s) {
      for (int i = 0; i < conf->nb_layers; ++i) {
        IAMF_FREE(conf->layer_conf_s[i].output_gain_info);
      }
      free(conf->layer_conf_s);
    }
    free(obj->channels_conf);
  } else if (obj->element_type == AUDIO_ELEMENT_TYPE_SCENE_BASED &&
             obj->ambisonics_conf) {
    IAMF_FREE(obj->ambisonics_conf->mapping);
    free(obj->ambisonics_conf);
  }

  free(obj);
}

IAMF_MixPresentation *iamf_mix_presentation_new(IAMF_OBU *obu) {
  IAMF_MixPresentation *mixp = 0;
  SubMixPresentation *sub = 0;
  OutputMixConf *output_mix_config;
  BitStream b;
  uint32_t val;

  mixp = IAMF_MALLOCZ(IAMF_MixPresentation, 1);
  if (!mixp) {
    ia_loge("fail to allocate memory for Mix Presentation Object.");
    goto mix_presentation_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  mixp->obj.type = IAMF_OBU_MIX_PRESENTATION;
  mixp->mix_presentation_id = bs_getAleb128(&b);
  mixp->mix_presentation_friendly_label =
      IAMF_MALLOCZ(char, iamf_obu_get_payload_size(obu));
  if (!mixp->mix_presentation_friendly_label) {
    ia_loge("fail to allocate memory for label of Mix Presentation Object.");
    goto mix_presentation_fail;
  }
  mixp->label_size = bs_readString(&b, mixp->mix_presentation_friendly_label,
                                   iamf_obu_get_payload_size(obu));

  mixp->num_sub_mixes = bs_getAleb128(&b);
  ia_logd(
      "Mix Presentation Object : id %lu, label %s, number of sub mixes %lu.",
      mixp->mix_presentation_id, mixp->mix_presentation_friendly_label,
      mixp->num_sub_mixes);

  if (mixp->num_sub_mixes != 1) {
    ia_loge("the total of sub mixes should be 1, not support %lu",
            mixp->num_sub_mixes);
    goto mix_presentation_fail;
  }

  // sub mixes;
  mixp->sub_mixes = IAMF_MALLOCZ(SubMixPresentation, mixp->num_sub_mixes);
  if (!mixp->sub_mixes) {
    ia_loge(
        "fail to allocate memory for sub mix presentation of Mix Presentation "
        "Object.");
    goto mix_presentation_fail;
  }

  for (int n = 0; n < mixp->num_sub_mixes; ++n) {
    sub = &mixp->sub_mixes[n];

    val = bs_getAleb128(&b);
    sub->nb_elements = val;
    ia_logd("element count %lu", sub->nb_elements);
    if (val) {
      ElementMixRenderConf *conf_s = IAMF_MALLOCZ(ElementMixRenderConf, val);
      if (!conf_s) {
        ia_loge(
            "fail to allocate memory for mixing and rendering config of Mix "
            "Presentation Object.");
        goto mix_presentation_fail;
      }
      sub->conf_s = conf_s;
      for (uint32_t i = 0; i < val; ++i) {
        conf_s[i].audio_element_friendly_label =
            IAMF_MALLOCZ(char, iamf_obu_get_payload_size(obu));
        if (!conf_s[i].audio_element_friendly_label) {
          ia_loge(
              "fail to allocate memory for audio element label of mixing and "
              "rendering config.");
          goto mix_presentation_fail;
        }
        conf_s[i].element_id = bs_getAleb128(&b);
        conf_s[i].label_size =
            bs_readString(&b, conf_s[i].audio_element_friendly_label,
                          iamf_obu_get_payload_size(obu));
        ia_logd("rendering info : element id %lu, label %s",
                conf_s[i].element_id, conf_s[i].audio_element_friendly_label);

        // rendering_config

        // element_mix_config
        if (iamf_parameter_base_init(&conf_s[i].conf_m.gain.base,
                                     IAMF_PARAMETER_TYPE_MIX_GAIN,
                                     &b) != IAMF_OK)
          goto mix_presentation_fail;
        conf_s[i].conf_m.gain.mix_gain = bs_getA16b(&b);
        ia_logd("element mix info : element mix gain 0x%x",
                conf_s[i].conf_m.gain.mix_gain & U16_MASK);
      }
    }

    // output_mix_config
    output_mix_config = &sub->output_mix_config;

    if (iamf_parameter_base_init(&output_mix_config->gain.base,
                                 IAMF_PARAMETER_TYPE_MIX_GAIN, &b) != IAMF_OK)
      goto mix_presentation_fail;
    output_mix_config->gain.mix_gain = bs_getA16b(&b);

    sub->num_layouts = bs_getAleb128(&b);
    ia_logd(
        "Output mix gain: id %lu, time base %lu, mix gain 0x%x, number layout "
        "%lu",
        output_mix_config->gain.base.id, output_mix_config->gain.base.rate,
        output_mix_config->gain.mix_gain & U16_MASK, sub->num_layouts);
    if (sub->num_layouts > 0) {
      TargetLayout **layouts = IAMF_MALLOCZ(TargetLayout *, sub->num_layouts);
      IAMF_LoudnessInfo *loudness =
          IAMF_MALLOCZ(IAMF_LoudnessInfo, sub->num_layouts);
      uint32_t type = 0;
      sub->layouts = layouts;
      sub->loudness = loudness;
      if (!layouts || !loudness) {
        ia_loge(
            "fail to allocate memory for layout and loudness of Mix "
            "Presentation Object.");
        goto mix_presentation_fail;
      }
      for (int i = 0; i < sub->num_layouts; i++) {
        // Layout
        type = bs_get32b(&b, 2);
        if (type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL) {
          SP_Label_Layout *sl = IAMF_MALLOCZ(SP_Label_Layout, 1);
          if (!sl) {
            ia_loge(
                "fail to allocate memory for sp label layout of Mix "
                "Presentation Object.");
            goto mix_presentation_fail;
          }
          layouts[i] = TARGET_LAYOUT(sl);
          sl->base.type = IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL;
          sl->nb_loudspeakers = bs_get32b(&b, 6);
          ia_logd("sp labels count %d", sl->nb_loudspeakers);
          if (sl->nb_loudspeakers > 0) {
            sl->sp_labels = IAMF_MALLOCZ(uint32_t, sl->nb_loudspeakers);
            if (!sl->sp_labels) {
              ia_loge(
                  "fail to allocate memory for sp labels of Mix Presentation "
                  "Object.");
              goto mix_presentation_fail;
            }
            for (int i = 0; i < sl->nb_loudspeakers; ++i) {
              sl->sp_labels[i] = bs_getA8b(&b);
              ia_logd("\t>sp label : %u", sl->sp_labels[i]);
            }
          }
        } else if (type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION) {
          SoundSystemLayout *ss = IAMF_MALLOCZ(SoundSystemLayout, 1);
          if (!ss) {
            ia_loge(
                "fail to allocate memory for sound system layout of Mix "
                "Presentation Object.");
            goto mix_presentation_fail;
          }
          layouts[i] = TARGET_LAYOUT(ss);
          ss->base.type = IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION;
          ss->sound_system = bs_get32b(&b, 4);
          ia_logd("\tLayout %d > sound system %d", i, ss->sound_system);
        } else if (type == IAMF_LAYOUT_TYPE_BINAURAL) {
          BinauralLayout *b = IAMF_MALLOCZ(BinauralLayout, 1);
          if (!b) {
            ia_loge(
                "fail to allocate memory for binaural layout of Mix "
                "Presentation Object.");
            goto mix_presentation_fail;
          }
          layouts[i] = TARGET_LAYOUT(b);
          ia_logd("\tLayout %d > binaural.", i);
        } else {
          ia_logw("Undefine layout type %d.", type);
        }
        bs_align(&b);

        // loudness
        loudness[i].info_type = bs_getA8b(&b);
        loudness[i].integrated_loudness = bs_getA16b(&b);
        loudness[i].digital_peak = bs_getA16b(&b);
        ia_logd(
            "\tLoudness : %d > info type 0x%x, integrated loudness 0x%x, "
            "digital peak 0x%x",
            i, loudness[i].info_type & U8_MASK,
            loudness[i].integrated_loudness & U16_MASK,
            loudness[i].digital_peak & U16_MASK);
        if (loudness[i].info_type & 1) {
          loudness[i].true_peak = bs_getA16b(&b);
          ia_logd("\tloudness > %d > true peak 0x%x", i,
                  loudness[i].true_peak & U16_MASK);
        }
      }
    }
  }

#if SR
  vlog_obu(IAMF_OBU_MIX_PRESENTATION, mixp, 0, 0);
#endif
  return mixp;

mix_presentation_fail:
  if (mixp) {
    iamf_mix_presentation_free(mixp);
  }
  return 0;
}

void iamf_mix_presentation_free(IAMF_MixPresentation *obj) {
  IAMF_FREE(obj->mix_presentation_friendly_label);

  if (obj->sub_mixes) {
    SubMixPresentation *sub;
    for (int i = 0; i < obj->num_sub_mixes; ++i) {
      sub = &obj->sub_mixes[i];

      if (sub->conf_s) {
        for (int i = 0; i < sub->nb_elements; ++i) {
          IAMF_FREE(sub->conf_s[i].audio_element_friendly_label);
          IAMF_FREE(sub->conf_s[i].conf_m.gain.base.segments);
        }
        free(sub->conf_s);
      }

      if (sub->layouts) {
        for (int i = 0; i < sub->num_layouts; ++i) {
          if (sub->layouts[i] &&
              sub->layouts[i]->type == IAMF_LAYOUT_TYPE_LOUDSPEAKERS_SP_LABEL) {
            SP_Label_Layout *sl = SP_LABEL_LAYOUT(sub->layouts[i]);
            free(sl->sp_labels);
          }
          free(sub->layouts[i]);
        }
        free(sub->layouts);
      }

      IAMF_FREE(sub->loudness);
    }

    free(obj->sub_mixes);
  }

  free(obj);
}

static uint64_t iamf_parameter_get_segment_interval(uint64_t duration,
                                                    uint64_t const_interval,
                                                    uint64_t interval) {
  if (interval) return interval;
  return const_interval < duration ? const_interval : duration;
}

IAMF_Parameter *iamf_parameter_new(IAMF_OBU *obu,
                                   IAMF_ParameterParam *objParam) {
  IAMF_Parameter *para = 0;
  ParameterSegment *seg;
  BitStream b;
  uint64_t interval = 0;
  uint64_t intervals;
  uint64_t segment_interval;

  if (!objParam || !objParam->param_base) {
    ia_loge("Invalid object parameters for Parameter Object.");
    goto parameter_fail;
  }

  ia_logd("parameter obu arguments: parameter type %lu",
          objParam->param_base->type);

  para = IAMF_MALLOCZ(IAMF_Parameter, 1);
  if (!para) {
    ia_loge("fail to allocate memory for Parameter Object.");
    goto parameter_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  para->obj.type = IAMF_OBU_PARAMETER_BLOCK;
  para->id = bs_getAleb128(&b);
  if (!objParam->param_base->mode) {
    intervals = para->duration = objParam->param_base->duration;
    para->nb_segments = objParam->param_base->nb_segments;
    para->constant_segment_interval =
        objParam->param_base->constant_segment_interval;
    para->type = objParam->param_base->type;
  } else {
    intervals = para->duration = bs_getAleb128(&b);
    para->nb_segments = bs_getAleb128(&b);
    para->constant_segment_interval = bs_getAleb128(&b);
    para->type = objParam->param_base->type;
  }

  ia_logd(
      "parameter id %lu, duration %lu, segment count %lu, const segment "
      "interval %lu, type %lu",
      para->id, para->duration, para->nb_segments,
      para->constant_segment_interval, para->type);

  para->segments = IAMF_MALLOCZ(ParameterSegment *, para->nb_segments);
  if (!para->segments) {
    ia_loge("fail to allocate segments for Parameter Object.");
    goto parameter_fail;
  }

  for (int i = 0; i < para->nb_segments; ++i) {
    if (!objParam->param_base->mode) {
      if (!para->constant_segment_interval) {
        interval = objParam->param_base->segments[i].segment_interval;
        ia_logd("parameter base segment interval %lu", interval);
      }
    } else {
      if (!para->constant_segment_interval) {
        interval = bs_getAleb128(&b);
        ia_logd("segment interval %lu", interval);
      }
    }

    segment_interval = iamf_parameter_get_segment_interval(
        intervals, para->constant_segment_interval, interval);
    intervals -= segment_interval;

    switch (para->type) {
      case IAMF_PARAMETER_TYPE_MIX_GAIN: {
        MixGainSegment *mg = IAMF_MALLOCZ(MixGainSegment, 1);
        float gain_db, gain1_db, gain2_db;
        if (!mg) {
          ia_loge("fail to allocate mix gain segment for Parameter Object.");
          goto parameter_fail;
        }

        mg->seg.type = IAMF_PARAMETER_TYPE_MIX_GAIN;
        seg = (ParameterSegment *)mg;
        para->segments[i] = seg;
        seg->segment_interval = segment_interval;
        mg->mix_gain_f.animated_type = mg->mix_gain.animated_type =
            bs_getAleb128(&b);
        mg->mix_gain.start = bs_getA16b(&b);
        gain_db = q_to_float(mg->mix_gain.start, 8);
        mg->mix_gain_f.start = db2lin(gain_db);
        if (mg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_STEP) {
          ia_logd(
              "\t mix gain seg %d: interval %lu, step, start %f(%fdb, "
              "<0x%02x>)",
              i, seg->segment_interval, mg->mix_gain_f.start, gain_db,
              mg->mix_gain.start & U16_MASK);
        } else {
          mg->mix_gain.end = bs_getA16b(&b);
          gain2_db = q_to_float(mg->mix_gain.end, 8);
          mg->mix_gain_f.end = db2lin(gain2_db);
          if (mg->mix_gain.animated_type == PARAMETER_ANIMATED_TYPE_LINEAR) {
            ia_logd(
                "\t mix gain seg %d: interval %lu, linear, start %f(%fdb, "
                "<0x%02x>), end %f(%fdb, <0x%02x>)",
                i, seg->segment_interval, mg->mix_gain_f.start, gain_db,
                mg->mix_gain.start & U16_MASK, mg->mix_gain_f.end, gain2_db,
                mg->mix_gain.end & U16_MASK);
          } else if (mg->mix_gain.animated_type ==
                     PARAMETER_ANIMATED_TYPE_BEZIER) {
            mg->mix_gain.control = bs_getA16b(&b);
            gain1_db = q_to_float(mg->mix_gain.control, 8);
            mg->mix_gain_f.control = db2lin(gain1_db);
            mg->mix_gain.control_relative_time = bs_getA8b(&b);
            mg->mix_gain_f.control_relative_time =
                qf_to_float(mg->mix_gain.control_relative_time, 8);
            ia_logd(
                "\t mix gain seg %d: interval %lu, bezier, start %f (%fdb "
                "<0x%02x>), end %f (%fdb <0x%02x>), control %f (%fdb "
                "<0x%02x>), control relative time %f (0x%x)",
                i, seg->segment_interval, mg->mix_gain_f.start, gain_db,
                mg->mix_gain.start & U16_MASK, mg->mix_gain_f.end, gain2_db,
                mg->mix_gain.end & U16_MASK, mg->mix_gain_f.control, gain1_db,
                mg->mix_gain.control & U16_MASK,
                mg->mix_gain_f.control_relative_time,
                mg->mix_gain.control_relative_time & U8_MASK);
          }
        }
      } break;
      case IAMF_PARAMETER_TYPE_DEMIXING: {
        DemixingSegment *mode = IAMF_MALLOC(DemixingSegment, 1);
        if (!mode) {
          ia_loge("fail to allocate demixing segment for Parameter Object.");
          goto parameter_fail;
        }
        mode->seg.type = IAMF_PARAMETER_TYPE_DEMIXING;
        seg = (ParameterSegment *)mode;
        para->segments[i] = seg;
        seg->segment_interval = segment_interval;
        mode->demixing_mode = bs_get32b(&b, 3);
        ia_logd("segment interval %lu, demixing mode : %d",
                seg->segment_interval, mode->demixing_mode);
      } break;
      case IAMF_PARAMETER_TYPE_RECON_GAIN: {
        ia_logd("recon gain, layer count %d", objParam->nb_layers);

        if (objParam->nb_layers > 0) {
          int count = objParam->nb_layers;
          ReconGainList *list;
          ReconGain *recon;
          ReconGainSegment *recon_gain;
          int channels = 0;

          recon_gain = IAMF_MALLOCZ(ReconGainSegment, 1);
          if (!recon_gain) {
            ia_loge(
                "fail to allocate Recon gain segment for Parameter Object.");
            goto parameter_fail;
          }

          recon_gain->seg.type = IAMF_PARAMETER_TYPE_RECON_GAIN;
          list = &recon_gain->list;

          seg = (ParameterSegment *)recon_gain;
          para->segments[i] = seg;
          seg->segment_interval = segment_interval;
          ia_logd("there are %d recon gain info, list is %p", count, list);
          list->count = count;
          recon = IAMF_MALLOCZ(ReconGain, list->count);
          list->recon = recon;
          if (!recon) {
            ia_loge("fail to allocate Recon gain for Parameter Object.");
            goto parameter_fail;
          }
          for (int k = 0; k < list->count; ++k) {
            recon[k].flags = bs_getAleb128(&b);
            channels = bit1_count(recon[k].flags);
            if (channels > 0) {
              recon[k].recon_gain = IAMF_MALLOCZ(uint8_t, channels);
              recon[k].recon_gain_f = IAMF_MALLOCZ(float, channels);
              if (!recon[k].recon_gain || !recon[k].recon_gain_f) {
                ia_loge(
                    "fail to allocate recon gain value for Parameter Object.");
                goto parameter_fail;
              }
              bs_read(&b, recon[k].recon_gain, channels);
              ia_logd("recon gain info %d : flags 0x%x, channels %d", k,
                      recon[k].flags, channels);
              for (int t = 0; t < channels; ++t) {
                recon[k].recon_gain_f[t] =
                    qf_to_float(recon[k].recon_gain[t], 8);
                ia_logd("\tch %d gain %f(0x%x)", t, recon[k].recon_gain_f[t],
                        recon[k].recon_gain[t] & U8_MASK);
              }
            }
          }
        }
      } break;
      default:
        ia_loge("Invalid parameter type for Parameter Object.");
        break;
    }
  }

#if SR
  vlog_obu(IAMF_OBU_PARAMETER_BLOCK, para, 0, 0);
#endif

  return para;

parameter_fail:
  if (para) {
    iamf_parameter_free(para);
  }

  return 0;
}

void iamf_parameter_free(IAMF_Parameter *obj) {
  if (obj->segments) {
    for (int i = 0; i < obj->nb_segments; ++i) {
      IAMF_parameter_segment_free(obj->segments[i]);
    }
    free(obj->segments);
  }
  free(obj);
}

IAMF_Frame *iamf_frame_new(IAMF_OBU *obu) {
  IAMF_Frame *pkt = 0;
  BitStream b;

  pkt = IAMF_MALLOCZ(IAMF_Frame, 1);
  if (!pkt) {
    ia_loge("fail to allocate memory for Audio Frame Object.");
    return 0;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  pkt->obj.type = obu->type;
  if (obu->type == IAMF_OBU_AUDIO_FRAME) {
    pkt->id = bs_getAleb128(&b);
  } else {
    pkt->id = obu->type - IAMF_OBU_AUDIO_FRAME_ID0;
  }
  pkt->trim_start = obu->trim_start;
  pkt->trim_end = obu->trim_end;
  pkt->data = obu->payload + bs_tell(&b);
  pkt->size = iamf_obu_get_payload_size(obu) - bs_tell(&b);

#if SR
  vlog_obu(IAMF_OBU_AUDIO_FRAME, pkt, obu->trim_start, obu->trim_end);
#endif
  return pkt;
}

IAMF_Sync *iamf_sync_new(IAMF_OBU *obu) {
  IAMF_Sync *sync = 0;
  BitStream b;

  sync = IAMF_MALLOCZ(IAMF_Sync, 1);
  if (!sync) {
    ia_loge("fail to allocate memory for Sync Object.");
    goto sync_fail;
  }

  bs(&b, obu->payload, iamf_obu_get_payload_size(obu));

  sync->obj.type = IAMF_OBU_SYNC;
  sync->global_offset = bs_getAleb128(&b);
  sync->nb_obu_ids = bs_getAleb128(&b);
  ia_logd("global offset %lu, numbers of obu %lu", sync->global_offset,
          sync->nb_obu_ids);
  if (sync->nb_obu_ids > 0) {
    sync->objs = IAMF_MALLOCZ(ObjectSync, sync->nb_obu_ids);
    if (!sync->objs) {
      ia_loge("fail to allocate object sync for Sync Object.");
      goto sync_fail;
    }

    for (int i = 0; i < sync->nb_obu_ids; ++i) {
      sync->objs[i].obu_id = bs_getAleb128(&b);
      sync->objs[i].obu_data_type = bs_get32b(&b, 1);
      sync->objs[i].reinitialize_decoder = bs_get32b(&b, 1);
      sync->objs[i].relative_offset = bs_getAsleb128(&b);
      ia_logd(
          "\t > %d : obu id %lu, data type %u, reset decoder %u, relative "
          "offset %ld",
          i, sync->objs[i].obu_id, sync->objs[i].obu_data_type,
          sync->objs[i].reinitialize_decoder, sync->objs[i].relative_offset);
    }
  }

#if SR
  vlog_obu(IAMF_OBU_SYNC, sync, 0, 0);
#endif
  return sync;

sync_fail:
  if (sync) {
    iamf_sync_free(sync);
  }

  return 0;
}

void iamf_sync_free(IAMF_Sync *obj) {
  IAMF_FREE(obj->objs);
  free(obj);
}

void iamf_parameter_recon_gain_segment_free(ReconGainSegment *seg) {
  if (seg->list.recon) {
    for (int i = 0; i < seg->list.count; ++i) {
      IAMF_FREE(seg->list.recon[i].recon_gain);
      IAMF_FREE(seg->list.recon[i].recon_gain_f);
    }
    free(seg->list.recon);
  }
  free(seg);
}
