/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <ctype.h>
#include <signal.h>

#include "alMain.h"
#include "alSource.h"
#include "AL/al.h"
#include "AL/alc.h"
#include "alThunk.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alAuxEffectSlot.h"
#include "alError.h"
#include "bs2b.h"
#include "alu.h"


/************************************************
 * Backends
 ************************************************/
#define EmptyFuncs { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
static struct BackendInfo BackendList[] = {
#ifdef HAVE_PULSEAUDIO
    { "pulse", alc_pulse_init, alc_pulse_deinit, alc_pulse_probe, EmptyFuncs },
#endif
#ifdef HAVE_ALSA
    { "alsa", alc_alsa_init, alc_alsa_deinit, alc_alsa_probe, EmptyFuncs },
#endif
#ifdef HAVE_COREAUDIO
    { "core", alc_ca_init, alc_ca_deinit, alc_ca_probe, EmptyFuncs },
#endif
#ifdef HAVE_OSS
    { "oss", alc_oss_init, alc_oss_deinit, alc_oss_probe, EmptyFuncs },
#endif
#ifdef HAVE_SOLARIS
    { "solaris", alc_solaris_init, alc_solaris_deinit, alc_solaris_probe, EmptyFuncs },
#endif
#ifdef HAVE_SNDIO
    { "sndio", alc_sndio_init, alc_sndio_deinit, alc_sndio_probe, EmptyFuncs },
#endif
#ifdef HAVE_MMDEVAPI
    { "mmdevapi", alcMMDevApiInit, alcMMDevApiDeinit, alcMMDevApiProbe, EmptyFuncs },
#endif
#ifdef HAVE_DSOUND
    { "dsound", alcDSoundInit, alcDSoundDeinit, alcDSoundProbe, EmptyFuncs },
#endif
#ifdef HAVE_WINMM
    { "winmm", alcWinMMInit, alcWinMMDeinit, alcWinMMProbe, EmptyFuncs },
#endif
#ifdef HAVE_PORTAUDIO
    { "port", alc_pa_init, alc_pa_deinit, alc_pa_probe, EmptyFuncs },
#endif
#ifdef HAVE_OPENSL
    { "opensl", alc_opensl_init, alc_opensl_deinit, alc_opensl_probe, EmptyFuncs },
#endif

    { "null", alc_null_init, alc_null_deinit, alc_null_probe, EmptyFuncs },
#ifdef HAVE_WAVE
    { "wave", alc_wave_init, alc_wave_deinit, alc_wave_probe, EmptyFuncs },
#endif

    { NULL, NULL, NULL, NULL, EmptyFuncs }
};
static struct BackendInfo BackendLoopback = {
    "loopback", alc_loopback_init, alc_loopback_deinit, alc_loopback_probe, EmptyFuncs
};
#undef EmptyFuncs

static struct BackendInfo PlaybackBackend;
static struct BackendInfo CaptureBackend;

/************************************************
 * Functions, enums, and errors
 ************************************************/
typedef struct ALCfunction {
    const ALCchar *funcName;
    ALCvoid *address;
} ALCfunction;

typedef struct ALCenums {
    const ALCchar *enumName;
    ALCenum value;
} ALCenums;

static const ALCfunction alcFunctions[] = {
    { "alcCreateContext",           (ALCvoid *) alcCreateContext         },
    { "alcMakeContextCurrent",      (ALCvoid *) alcMakeContextCurrent    },
    { "alcProcessContext",          (ALCvoid *) alcProcessContext        },
    { "alcSuspendContext",          (ALCvoid *) alcSuspendContext        },
    { "alcDestroyContext",          (ALCvoid *) alcDestroyContext        },
    { "alcGetCurrentContext",       (ALCvoid *) alcGetCurrentContext     },
    { "alcGetContextsDevice",       (ALCvoid *) alcGetContextsDevice     },
    { "alcOpenDevice",              (ALCvoid *) alcOpenDevice            },
    { "alcCloseDevice",             (ALCvoid *) alcCloseDevice           },
    { "alcGetError",                (ALCvoid *) alcGetError              },
    { "alcIsExtensionPresent",      (ALCvoid *) alcIsExtensionPresent    },
    { "alcGetProcAddress",          (ALCvoid *) alcGetProcAddress        },
    { "alcGetEnumValue",            (ALCvoid *) alcGetEnumValue          },
    { "alcGetString",               (ALCvoid *) alcGetString             },
    { "alcGetIntegerv",             (ALCvoid *) alcGetIntegerv           },
    { "alcCaptureOpenDevice",       (ALCvoid *) alcCaptureOpenDevice     },
    { "alcCaptureCloseDevice",      (ALCvoid *) alcCaptureCloseDevice    },
    { "alcCaptureStart",            (ALCvoid *) alcCaptureStart          },
    { "alcCaptureStop",             (ALCvoid *) alcCaptureStop           },
    { "alcCaptureSamples",          (ALCvoid *) alcCaptureSamples        },

    { "alcSetThreadContext",        (ALCvoid *) alcSetThreadContext      },
    { "alcGetThreadContext",        (ALCvoid *) alcGetThreadContext      },

    { "alcLoopbackOpenDeviceSOFT",  (ALCvoid *) alcLoopbackOpenDeviceSOFT},
    { "alcIsRenderFormatSupportedSOFT",(ALCvoid *) alcIsRenderFormatSupportedSOFT},
    { "alcRenderSamplesSOFT",       (ALCvoid *) alcRenderSamplesSOFT         },

    { "alEnable",                   (ALCvoid *) alEnable                 },
    { "alDisable",                  (ALCvoid *) alDisable                },
    { "alIsEnabled",                (ALCvoid *) alIsEnabled              },
    { "alGetString",                (ALCvoid *) alGetString              },
    { "alGetBooleanv",              (ALCvoid *) alGetBooleanv            },
    { "alGetIntegerv",              (ALCvoid *) alGetIntegerv            },
    { "alGetFloatv",                (ALCvoid *) alGetFloatv              },
    { "alGetDoublev",               (ALCvoid *) alGetDoublev             },
    { "alGetBoolean",               (ALCvoid *) alGetBoolean             },
    { "alGetInteger",               (ALCvoid *) alGetInteger             },
    { "alGetFloat",                 (ALCvoid *) alGetFloat               },
    { "alGetDouble",                (ALCvoid *) alGetDouble              },
    { "alGetError",                 (ALCvoid *) alGetError               },
    { "alIsExtensionPresent",       (ALCvoid *) alIsExtensionPresent     },
    { "alGetProcAddress",           (ALCvoid *) alGetProcAddress         },
    { "alGetEnumValue",             (ALCvoid *) alGetEnumValue           },
    { "alListenerf",                (ALCvoid *) alListenerf              },
    { "alListener3f",               (ALCvoid *) alListener3f             },
    { "alListenerfv",               (ALCvoid *) alListenerfv             },
    { "alListeneri",                (ALCvoid *) alListeneri              },
    { "alListener3i",               (ALCvoid *) alListener3i             },
    { "alListeneriv",               (ALCvoid *) alListeneriv             },
    { "alGetListenerf",             (ALCvoid *) alGetListenerf           },
    { "alGetListener3f",            (ALCvoid *) alGetListener3f          },
    { "alGetListenerfv",            (ALCvoid *) alGetListenerfv          },
    { "alGetListeneri",             (ALCvoid *) alGetListeneri           },
    { "alGetListener3i",            (ALCvoid *) alGetListener3i          },
    { "alGetListeneriv",            (ALCvoid *) alGetListeneriv          },
    { "alGenSources",               (ALCvoid *) alGenSources             },
    { "alDeleteSources",            (ALCvoid *) alDeleteSources          },
    { "alIsSource",                 (ALCvoid *) alIsSource               },
    { "alSourcef",                  (ALCvoid *) alSourcef                },
    { "alSource3f",                 (ALCvoid *) alSource3f               },
    { "alSourcefv",                 (ALCvoid *) alSourcefv               },
    { "alSourcei",                  (ALCvoid *) alSourcei                },
    { "alSource3i",                 (ALCvoid *) alSource3i               },
    { "alSourceiv",                 (ALCvoid *) alSourceiv               },
    { "alGetSourcef",               (ALCvoid *) alGetSourcef             },
    { "alGetSource3f",              (ALCvoid *) alGetSource3f            },
    { "alGetSourcefv",              (ALCvoid *) alGetSourcefv            },
    { "alGetSourcei",               (ALCvoid *) alGetSourcei             },
    { "alGetSource3i",              (ALCvoid *) alGetSource3i            },
    { "alGetSourceiv",              (ALCvoid *) alGetSourceiv            },
    { "alSourcePlayv",              (ALCvoid *) alSourcePlayv            },
    { "alSourceStopv",              (ALCvoid *) alSourceStopv            },
    { "alSourceRewindv",            (ALCvoid *) alSourceRewindv          },
    { "alSourcePausev",             (ALCvoid *) alSourcePausev           },
    { "alSourcePlay",               (ALCvoid *) alSourcePlay             },
    { "alSourceStop",               (ALCvoid *) alSourceStop             },
    { "alSourceRewind",             (ALCvoid *) alSourceRewind           },
    { "alSourcePause",              (ALCvoid *) alSourcePause            },
    { "alSourceQueueBuffers",       (ALCvoid *) alSourceQueueBuffers     },
    { "alSourceUnqueueBuffers",     (ALCvoid *) alSourceUnqueueBuffers   },
    { "alGenBuffers",               (ALCvoid *) alGenBuffers             },
    { "alDeleteBuffers",            (ALCvoid *) alDeleteBuffers          },
    { "alIsBuffer",                 (ALCvoid *) alIsBuffer               },
    { "alBufferData",               (ALCvoid *) alBufferData             },
    { "alBufferf",                  (ALCvoid *) alBufferf                },
    { "alBuffer3f",                 (ALCvoid *) alBuffer3f               },
    { "alBufferfv",                 (ALCvoid *) alBufferfv               },
    { "alBufferi",                  (ALCvoid *) alBufferi                },
    { "alBuffer3i",                 (ALCvoid *) alBuffer3i               },
    { "alBufferiv",                 (ALCvoid *) alBufferiv               },
    { "alGetBufferf",               (ALCvoid *) alGetBufferf             },
    { "alGetBuffer3f",              (ALCvoid *) alGetBuffer3f            },
    { "alGetBufferfv",              (ALCvoid *) alGetBufferfv            },
    { "alGetBufferi",               (ALCvoid *) alGetBufferi             },
    { "alGetBuffer3i",              (ALCvoid *) alGetBuffer3i            },
    { "alGetBufferiv",              (ALCvoid *) alGetBufferiv            },
    { "alDopplerFactor",            (ALCvoid *) alDopplerFactor          },
    { "alDopplerVelocity",          (ALCvoid *) alDopplerVelocity        },
    { "alSpeedOfSound",             (ALCvoid *) alSpeedOfSound           },
    { "alDistanceModel",            (ALCvoid *) alDistanceModel          },

    { "alGenFilters",               (ALCvoid *) alGenFilters             },
    { "alDeleteFilters",            (ALCvoid *) alDeleteFilters          },
    { "alIsFilter",                 (ALCvoid *) alIsFilter               },
    { "alFilteri",                  (ALCvoid *) alFilteri                },
    { "alFilteriv",                 (ALCvoid *) alFilteriv               },
    { "alFilterf",                  (ALCvoid *) alFilterf                },
    { "alFilterfv",                 (ALCvoid *) alFilterfv               },
    { "alGetFilteri",               (ALCvoid *) alGetFilteri             },
    { "alGetFilteriv",              (ALCvoid *) alGetFilteriv            },
    { "alGetFilterf",               (ALCvoid *) alGetFilterf             },
    { "alGetFilterfv",              (ALCvoid *) alGetFilterfv            },
    { "alGenEffects",               (ALCvoid *) alGenEffects             },
    { "alDeleteEffects",            (ALCvoid *) alDeleteEffects          },
    { "alIsEffect",                 (ALCvoid *) alIsEffect               },
    { "alEffecti",                  (ALCvoid *) alEffecti                },
    { "alEffectiv",                 (ALCvoid *) alEffectiv               },
    { "alEffectf",                  (ALCvoid *) alEffectf                },
    { "alEffectfv",                 (ALCvoid *) alEffectfv               },
    { "alGetEffecti",               (ALCvoid *) alGetEffecti             },
    { "alGetEffectiv",              (ALCvoid *) alGetEffectiv            },
    { "alGetEffectf",               (ALCvoid *) alGetEffectf             },
    { "alGetEffectfv",              (ALCvoid *) alGetEffectfv            },
    { "alGenAuxiliaryEffectSlots",  (ALCvoid *) alGenAuxiliaryEffectSlots},
    { "alDeleteAuxiliaryEffectSlots",(ALCvoid *) alDeleteAuxiliaryEffectSlots},
    { "alIsAuxiliaryEffectSlot",    (ALCvoid *) alIsAuxiliaryEffectSlot  },
    { "alAuxiliaryEffectSloti",     (ALCvoid *) alAuxiliaryEffectSloti   },
    { "alAuxiliaryEffectSlotiv",    (ALCvoid *) alAuxiliaryEffectSlotiv  },
    { "alAuxiliaryEffectSlotf",     (ALCvoid *) alAuxiliaryEffectSlotf   },
    { "alAuxiliaryEffectSlotfv",    (ALCvoid *) alAuxiliaryEffectSlotfv  },
    { "alGetAuxiliaryEffectSloti",  (ALCvoid *) alGetAuxiliaryEffectSloti},
    { "alGetAuxiliaryEffectSlotiv", (ALCvoid *) alGetAuxiliaryEffectSlotiv},
    { "alGetAuxiliaryEffectSlotf",  (ALCvoid *) alGetAuxiliaryEffectSlotf},
    { "alGetAuxiliaryEffectSlotfv", (ALCvoid *) alGetAuxiliaryEffectSlotfv},

    { "alBufferSubDataSOFT",        (ALCvoid *) alBufferSubDataSOFT      },

    { "alBufferSamplesSOFT",        (ALCvoid *) alBufferSamplesSOFT      },
    { "alBufferSubSamplesSOFT",     (ALCvoid *) alBufferSubSamplesSOFT   },
    { "alGetBufferSamplesSOFT",     (ALCvoid *) alGetBufferSamplesSOFT   },
    { "alIsBufferFormatSupportedSOFT",(ALCvoid *) alIsBufferFormatSupportedSOFT},

    { "alDeferUpdatesSOFT",         (ALCvoid *) alDeferUpdatesSOFT       },
    { "alProcessUpdatesSOFT",       (ALCvoid *) alProcessUpdatesSOFT     },

    { NULL,                         (ALCvoid *) NULL                     }
};

static const ALCenums enumeration[] = {
    { "ALC_INVALID",                          ALC_INVALID                         },
    { "ALC_FALSE",                            ALC_FALSE                           },
    { "ALC_TRUE",                             ALC_TRUE                            },

    { "ALC_MAJOR_VERSION",                    ALC_MAJOR_VERSION                   },
    { "ALC_MINOR_VERSION",                    ALC_MINOR_VERSION                   },
    { "ALC_ATTRIBUTES_SIZE",                  ALC_ATTRIBUTES_SIZE                 },
    { "ALC_ALL_ATTRIBUTES",                   ALC_ALL_ATTRIBUTES                  },
    { "ALC_DEFAULT_DEVICE_SPECIFIER",         ALC_DEFAULT_DEVICE_SPECIFIER        },
    { "ALC_DEVICE_SPECIFIER",                 ALC_DEVICE_SPECIFIER                },
    { "ALC_ALL_DEVICES_SPECIFIER",            ALC_ALL_DEVICES_SPECIFIER           },
    { "ALC_DEFAULT_ALL_DEVICES_SPECIFIER",    ALC_DEFAULT_ALL_DEVICES_SPECIFIER   },
    { "ALC_EXTENSIONS",                       ALC_EXTENSIONS                      },
    { "ALC_FREQUENCY",                        ALC_FREQUENCY                       },
    { "ALC_REFRESH",                          ALC_REFRESH                         },
    { "ALC_SYNC",                             ALC_SYNC                            },
    { "ALC_MONO_SOURCES",                     ALC_MONO_SOURCES                    },
    { "ALC_STEREO_SOURCES",                   ALC_STEREO_SOURCES                  },
    { "ALC_CAPTURE_DEVICE_SPECIFIER",         ALC_CAPTURE_DEVICE_SPECIFIER        },
    { "ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER", ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER},
    { "ALC_CAPTURE_SAMPLES",                  ALC_CAPTURE_SAMPLES                 },
    { "ALC_CONNECTED",                        ALC_CONNECTED                       },

    { "ALC_EFX_MAJOR_VERSION",                ALC_EFX_MAJOR_VERSION               },
    { "ALC_EFX_MINOR_VERSION",                ALC_EFX_MINOR_VERSION               },
    { "ALC_MAX_AUXILIARY_SENDS",              ALC_MAX_AUXILIARY_SENDS             },

    { "ALC_FORMAT_CHANNELS_SOFT",             ALC_FORMAT_CHANNELS_SOFT            },
    { "ALC_FORMAT_TYPE_SOFT",                 ALC_FORMAT_TYPE_SOFT                },

    { "ALC_MONO_SOFT",                        ALC_MONO_SOFT                       },
    { "ALC_STEREO_SOFT",                      ALC_STEREO_SOFT                     },
    { "ALC_QUAD_SOFT",                        ALC_QUAD_SOFT                       },
    { "ALC_5POINT1_SOFT",                     ALC_5POINT1_SOFT                    },
    { "ALC_6POINT1_SOFT",                     ALC_6POINT1_SOFT                    },
    { "ALC_7POINT1_SOFT",                     ALC_7POINT1_SOFT                    },

    { "ALC_BYTE_SOFT",                        ALC_BYTE_SOFT                       },
    { "ALC_UNSIGNED_BYTE_SOFT",               ALC_UNSIGNED_BYTE_SOFT              },
    { "ALC_SHORT_SOFT",                       ALC_SHORT_SOFT                      },
    { "ALC_UNSIGNED_SHORT_SOFT",              ALC_UNSIGNED_SHORT_SOFT             },
    { "ALC_INT_SOFT",                         ALC_INT_SOFT                        },
    { "ALC_UNSIGNED_INT_SOFT",                ALC_UNSIGNED_INT_SOFT               },
    { "ALC_FLOAT_SOFT",                       ALC_FLOAT_SOFT                      },

    { "ALC_NO_ERROR",                         ALC_NO_ERROR                        },
    { "ALC_INVALID_DEVICE",                   ALC_INVALID_DEVICE                  },
    { "ALC_INVALID_CONTEXT",                  ALC_INVALID_CONTEXT                 },
    { "ALC_INVALID_ENUM",                     ALC_INVALID_ENUM                    },
    { "ALC_INVALID_VALUE",                    ALC_INVALID_VALUE                   },
    { "ALC_OUT_OF_MEMORY",                    ALC_OUT_OF_MEMORY                   },


    { "AL_INVALID",                           AL_INVALID                          },
    { "AL_NONE",                              AL_NONE                             },
    { "AL_FALSE",                             AL_FALSE                            },
    { "AL_TRUE",                              AL_TRUE                             },

    { "AL_SOURCE_RELATIVE",                   AL_SOURCE_RELATIVE                  },
    { "AL_CONE_INNER_ANGLE",                  AL_CONE_INNER_ANGLE                 },
    { "AL_CONE_OUTER_ANGLE",                  AL_CONE_OUTER_ANGLE                 },
    { "AL_PITCH",                             AL_PITCH                            },
    { "AL_POSITION",                          AL_POSITION                         },
    { "AL_DIRECTION",                         AL_DIRECTION                        },
    { "AL_VELOCITY",                          AL_VELOCITY                         },
    { "AL_LOOPING",                           AL_LOOPING                          },
    { "AL_BUFFER",                            AL_BUFFER                           },
    { "AL_GAIN",                              AL_GAIN                             },
    { "AL_MIN_GAIN",                          AL_MIN_GAIN                         },
    { "AL_MAX_GAIN",                          AL_MAX_GAIN                         },
    { "AL_ORIENTATION",                       AL_ORIENTATION                      },
    { "AL_REFERENCE_DISTANCE",                AL_REFERENCE_DISTANCE               },
    { "AL_ROLLOFF_FACTOR",                    AL_ROLLOFF_FACTOR                   },
    { "AL_CONE_OUTER_GAIN",                   AL_CONE_OUTER_GAIN                  },
    { "AL_MAX_DISTANCE",                      AL_MAX_DISTANCE                     },
    { "AL_SEC_OFFSET",                        AL_SEC_OFFSET                       },
    { "AL_SAMPLE_OFFSET",                     AL_SAMPLE_OFFSET                    },
    { "AL_SAMPLE_RW_OFFSETS_SOFT",            AL_SAMPLE_RW_OFFSETS_SOFT           },
    { "AL_BYTE_OFFSET",                       AL_BYTE_OFFSET                      },
    { "AL_BYTE_RW_OFFSETS_SOFT",              AL_BYTE_RW_OFFSETS_SOFT             },
    { "AL_SOURCE_TYPE",                       AL_SOURCE_TYPE                      },
    { "AL_STATIC",                            AL_STATIC                           },
    { "AL_STREAMING",                         AL_STREAMING                        },
    { "AL_UNDETERMINED",                      AL_UNDETERMINED                     },
    { "AL_METERS_PER_UNIT",                   AL_METERS_PER_UNIT                  },
    { "AL_DIRECT_CHANNELS_SOFT",              AL_DIRECT_CHANNELS_SOFT             },

    { "AL_DIRECT_FILTER",                     AL_DIRECT_FILTER                    },
    { "AL_AUXILIARY_SEND_FILTER",             AL_AUXILIARY_SEND_FILTER            },
    { "AL_AIR_ABSORPTION_FACTOR",             AL_AIR_ABSORPTION_FACTOR            },
    { "AL_ROOM_ROLLOFF_FACTOR",               AL_ROOM_ROLLOFF_FACTOR              },
    { "AL_CONE_OUTER_GAINHF",                 AL_CONE_OUTER_GAINHF                },
    { "AL_DIRECT_FILTER_GAINHF_AUTO",         AL_DIRECT_FILTER_GAINHF_AUTO        },
    { "AL_AUXILIARY_SEND_FILTER_GAIN_AUTO",   AL_AUXILIARY_SEND_FILTER_GAIN_AUTO  },
    { "AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO", AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO},

    { "AL_SOURCE_STATE",                      AL_SOURCE_STATE                     },
    { "AL_INITIAL",                           AL_INITIAL                          },
    { "AL_PLAYING",                           AL_PLAYING                          },
    { "AL_PAUSED",                            AL_PAUSED                           },
    { "AL_STOPPED",                           AL_STOPPED                          },

    { "AL_BUFFERS_QUEUED",                    AL_BUFFERS_QUEUED                   },
    { "AL_BUFFERS_PROCESSED",                 AL_BUFFERS_PROCESSED                },

    { "AL_FORMAT_MONO8",                      AL_FORMAT_MONO8                     },
    { "AL_FORMAT_MONO16",                     AL_FORMAT_MONO16                    },
    { "AL_FORMAT_MONO_FLOAT32",               AL_FORMAT_MONO_FLOAT32              },
    { "AL_FORMAT_MONO_DOUBLE_EXT",            AL_FORMAT_MONO_DOUBLE_EXT           },
    { "AL_FORMAT_STEREO8",                    AL_FORMAT_STEREO8                   },
    { "AL_FORMAT_STEREO16",                   AL_FORMAT_STEREO16                  },
    { "AL_FORMAT_STEREO_FLOAT32",             AL_FORMAT_STEREO_FLOAT32            },
    { "AL_FORMAT_STEREO_DOUBLE_EXT",          AL_FORMAT_STEREO_DOUBLE_EXT         },
    { "AL_FORMAT_MONO_IMA4",                  AL_FORMAT_MONO_IMA4                 },
    { "AL_FORMAT_STEREO_IMA4",                AL_FORMAT_STEREO_IMA4               },
    { "AL_FORMAT_QUAD8_LOKI",                 AL_FORMAT_QUAD8_LOKI                },
    { "AL_FORMAT_QUAD16_LOKI",                AL_FORMAT_QUAD16_LOKI               },
    { "AL_FORMAT_QUAD8",                      AL_FORMAT_QUAD8                     },
    { "AL_FORMAT_QUAD16",                     AL_FORMAT_QUAD16                    },
    { "AL_FORMAT_QUAD32",                     AL_FORMAT_QUAD32                    },
    { "AL_FORMAT_51CHN8",                     AL_FORMAT_51CHN8                    },
    { "AL_FORMAT_51CHN16",                    AL_FORMAT_51CHN16                   },
    { "AL_FORMAT_51CHN32",                    AL_FORMAT_51CHN32                   },
    { "AL_FORMAT_61CHN8",                     AL_FORMAT_61CHN8                    },
    { "AL_FORMAT_61CHN16",                    AL_FORMAT_61CHN16                   },
    { "AL_FORMAT_61CHN32",                    AL_FORMAT_61CHN32                   },
    { "AL_FORMAT_71CHN8",                     AL_FORMAT_71CHN8                    },
    { "AL_FORMAT_71CHN16",                    AL_FORMAT_71CHN16                   },
    { "AL_FORMAT_71CHN32",                    AL_FORMAT_71CHN32                   },
    { "AL_FORMAT_REAR8",                      AL_FORMAT_REAR8                     },
    { "AL_FORMAT_REAR16",                     AL_FORMAT_REAR16                    },
    { "AL_FORMAT_REAR32",                     AL_FORMAT_REAR32                    },
    { "AL_FORMAT_MONO_MULAW",                 AL_FORMAT_MONO_MULAW                },
    { "AL_FORMAT_MONO_MULAW_EXT",             AL_FORMAT_MONO_MULAW_EXT            },
    { "AL_FORMAT_STEREO_MULAW",               AL_FORMAT_STEREO_MULAW              },
    { "AL_FORMAT_STEREO_MULAW_EXT",           AL_FORMAT_STEREO_MULAW_EXT          },
    { "AL_FORMAT_QUAD_MULAW",                 AL_FORMAT_QUAD_MULAW                },
    { "AL_FORMAT_51CHN_MULAW",                AL_FORMAT_51CHN_MULAW               },
    { "AL_FORMAT_61CHN_MULAW",                AL_FORMAT_61CHN_MULAW               },
    { "AL_FORMAT_71CHN_MULAW",                AL_FORMAT_71CHN_MULAW               },
    { "AL_FORMAT_REAR_MULAW",                 AL_FORMAT_REAR_MULAW                },
    { "AL_FORMAT_MONO_ALAW_EXT",              AL_FORMAT_MONO_ALAW_EXT             },
    { "AL_FORMAT_STEREO_ALAW_EXT",            AL_FORMAT_STEREO_ALAW_EXT           },

    { "AL_MONO8_SOFT",                        AL_MONO8_SOFT                       },
    { "AL_MONO16_SOFT",                       AL_MONO16_SOFT                      },
    { "AL_MONO32F_SOFT",                      AL_MONO32F_SOFT                     },
    { "AL_STEREO8_SOFT",                      AL_STEREO8_SOFT                     },
    { "AL_STEREO16_SOFT",                     AL_STEREO16_SOFT                    },
    { "AL_STEREO32F_SOFT",                    AL_STEREO32F_SOFT                   },
    { "AL_QUAD8_SOFT",                        AL_QUAD8_SOFT                       },
    { "AL_QUAD16_SOFT",                       AL_QUAD16_SOFT                      },
    { "AL_QUAD32F_SOFT",                      AL_QUAD32F_SOFT                     },
    { "AL_REAR8_SOFT",                        AL_REAR8_SOFT                       },
    { "AL_REAR16_SOFT",                       AL_REAR16_SOFT                      },
    { "AL_REAR32F_SOFT",                      AL_REAR32F_SOFT                     },
    { "AL_5POINT1_8_SOFT",                    AL_5POINT1_8_SOFT                   },
    { "AL_5POINT1_16_SOFT",                   AL_5POINT1_16_SOFT                  },
    { "AL_5POINT1_32F_SOFT",                  AL_5POINT1_32F_SOFT                 },
    { "AL_6POINT1_8_SOFT",                    AL_6POINT1_8_SOFT                   },
    { "AL_6POINT1_16_SOFT",                   AL_6POINT1_16_SOFT                  },
    { "AL_6POINT1_32F_SOFT",                  AL_6POINT1_32F_SOFT                 },
    { "AL_7POINT1_8_SOFT",                    AL_7POINT1_8_SOFT                   },
    { "AL_7POINT1_16_SOFT",                   AL_7POINT1_16_SOFT                  },
    { "AL_7POINT1_32F_SOFT",                  AL_7POINT1_32F_SOFT                 },

    { "AL_MONO_SOFT",                         AL_MONO_SOFT                        },
    { "AL_STEREO_SOFT",                       AL_STEREO_SOFT                      },
    { "AL_QUAD_SOFT",                         AL_QUAD_SOFT                        },
    { "AL_REAR_SOFT",                         AL_REAR_SOFT                        },
    { "AL_5POINT1_SOFT",                      AL_5POINT1_SOFT                     },
    { "AL_6POINT1_SOFT",                      AL_6POINT1_SOFT                     },
    { "AL_7POINT1_SOFT",                      AL_7POINT1_SOFT                     },

    { "AL_BYTE_SOFT",                         AL_BYTE_SOFT                        },
    { "AL_UNSIGNED_BYTE_SOFT",                AL_UNSIGNED_BYTE_SOFT               },
    { "AL_SHORT_SOFT",                        AL_SHORT_SOFT                       },
    { "AL_UNSIGNED_SHORT_SOFT",               AL_UNSIGNED_SHORT_SOFT              },
    { "AL_INT_SOFT",                          AL_INT_SOFT                         },
    { "AL_UNSIGNED_INT_SOFT",                 AL_UNSIGNED_INT_SOFT                },
    { "AL_FLOAT_SOFT",                        AL_FLOAT_SOFT                       },
    { "AL_DOUBLE_SOFT",                       AL_DOUBLE_SOFT                      },
    { "AL_BYTE3_SOFT",                        AL_BYTE3_SOFT                       },
    { "AL_UNSIGNED_BYTE3_SOFT",               AL_UNSIGNED_BYTE3_SOFT              },

    { "AL_FREQUENCY",                         AL_FREQUENCY                        },
    { "AL_BITS",                              AL_BITS                             },
    { "AL_CHANNELS",                          AL_CHANNELS                         },
    { "AL_SIZE",                              AL_SIZE                             },
    { "AL_INTERNAL_FORMAT_SOFT",              AL_INTERNAL_FORMAT_SOFT             },
    { "AL_BYTE_LENGTH_SOFT",                  AL_BYTE_LENGTH_SOFT                 },
    { "AL_SAMPLE_LENGTH_SOFT",                AL_SAMPLE_LENGTH_SOFT               },
    { "AL_SEC_LENGTH_SOFT",                   AL_SEC_LENGTH_SOFT                  },

    { "AL_UNUSED",                            AL_UNUSED                           },
    { "AL_PENDING",                           AL_PENDING                          },
    { "AL_PROCESSED",                         AL_PROCESSED                        },

    { "AL_NO_ERROR",                          AL_NO_ERROR                         },
    { "AL_INVALID_NAME",                      AL_INVALID_NAME                     },
    { "AL_INVALID_ENUM",                      AL_INVALID_ENUM                     },
    { "AL_INVALID_VALUE",                     AL_INVALID_VALUE                    },
    { "AL_INVALID_OPERATION",                 AL_INVALID_OPERATION                },
    { "AL_OUT_OF_MEMORY",                     AL_OUT_OF_MEMORY                    },

    { "AL_VENDOR",                            AL_VENDOR                           },
    { "AL_VERSION",                           AL_VERSION                          },
    { "AL_RENDERER",                          AL_RENDERER                         },
    { "AL_EXTENSIONS",                        AL_EXTENSIONS                       },

    { "AL_DOPPLER_FACTOR",                    AL_DOPPLER_FACTOR                   },
    { "AL_DOPPLER_VELOCITY",                  AL_DOPPLER_VELOCITY                 },
    { "AL_DISTANCE_MODEL",                    AL_DISTANCE_MODEL                   },
    { "AL_SPEED_OF_SOUND",                    AL_SPEED_OF_SOUND                   },
    { "AL_SOURCE_DISTANCE_MODEL",             AL_SOURCE_DISTANCE_MODEL            },
    { "AL_DEFERRED_UPDATES_SOFT",             AL_DEFERRED_UPDATES_SOFT            },

    { "AL_INVERSE_DISTANCE",                  AL_INVERSE_DISTANCE                 },
    { "AL_INVERSE_DISTANCE_CLAMPED",          AL_INVERSE_DISTANCE_CLAMPED         },
    { "AL_LINEAR_DISTANCE",                   AL_LINEAR_DISTANCE                  },
    { "AL_LINEAR_DISTANCE_CLAMPED",           AL_LINEAR_DISTANCE_CLAMPED          },
    { "AL_EXPONENT_DISTANCE",                 AL_EXPONENT_DISTANCE                },
    { "AL_EXPONENT_DISTANCE_CLAMPED",         AL_EXPONENT_DISTANCE_CLAMPED        },

    { "AL_FILTER_TYPE",                       AL_FILTER_TYPE                      },
    { "AL_FILTER_NULL",                       AL_FILTER_NULL                      },
    { "AL_FILTER_LOWPASS",                    AL_FILTER_LOWPASS                   },
#if 0
    { "AL_FILTER_HIGHPASS",                   AL_FILTER_HIGHPASS                  },
    { "AL_FILTER_BANDPASS",                   AL_FILTER_BANDPASS                  },
#endif

    { "AL_LOWPASS_GAIN",                      AL_LOWPASS_GAIN                     },
    { "AL_LOWPASS_GAINHF",                    AL_LOWPASS_GAINHF                   },

    { "AL_EFFECT_TYPE",                       AL_EFFECT_TYPE                      },
    { "AL_EFFECT_NULL",                       AL_EFFECT_NULL                      },
    { "AL_EFFECT_REVERB",                     AL_EFFECT_REVERB                    },
    { "AL_EFFECT_EAXREVERB",                  AL_EFFECT_EAXREVERB                 },
#if 0
    { "AL_EFFECT_CHORUS",                     AL_EFFECT_CHORUS                    },
    { "AL_EFFECT_DISTORTION",                 AL_EFFECT_DISTORTION                },
#endif
    { "AL_EFFECT_ECHO",                       AL_EFFECT_ECHO                      },
#if 0
    { "AL_EFFECT_FLANGER",                    AL_EFFECT_FLANGER                   },
    { "AL_EFFECT_FREQUENCY_SHIFTER",          AL_EFFECT_FREQUENCY_SHIFTER         },
    { "AL_EFFECT_VOCAL_MORPHER",              AL_EFFECT_VOCAL_MORPHER             },
    { "AL_EFFECT_PITCH_SHIFTER",              AL_EFFECT_PITCH_SHIFTER             },
#endif
    { "AL_EFFECT_RING_MODULATOR",             AL_EFFECT_RING_MODULATOR            },
#if 0
    { "AL_EFFECT_AUTOWAH",                    AL_EFFECT_AUTOWAH                   },
    { "AL_EFFECT_COMPRESSOR",                 AL_EFFECT_COMPRESSOR                },
    { "AL_EFFECT_EQUALIZER",                  AL_EFFECT_EQUALIZER                 },
#endif
    { "AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT",AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT},
    { "AL_EFFECT_DEDICATED_DIALOGUE",         AL_EFFECT_DEDICATED_DIALOGUE        },

    { "AL_EAXREVERB_DENSITY",                 AL_EAXREVERB_DENSITY                },
    { "AL_EAXREVERB_DIFFUSION",               AL_EAXREVERB_DIFFUSION              },
    { "AL_EAXREVERB_GAIN",                    AL_EAXREVERB_GAIN                   },
    { "AL_EAXREVERB_GAINHF",                  AL_EAXREVERB_GAINHF                 },
    { "AL_EAXREVERB_GAINLF",                  AL_EAXREVERB_GAINLF                 },
    { "AL_EAXREVERB_DECAY_TIME",              AL_EAXREVERB_DECAY_TIME             },
    { "AL_EAXREVERB_DECAY_HFRATIO",           AL_EAXREVERB_DECAY_HFRATIO          },
    { "AL_EAXREVERB_DECAY_LFRATIO",           AL_EAXREVERB_DECAY_LFRATIO          },
    { "AL_EAXREVERB_REFLECTIONS_GAIN",        AL_EAXREVERB_REFLECTIONS_GAIN       },
    { "AL_EAXREVERB_REFLECTIONS_DELAY",       AL_EAXREVERB_REFLECTIONS_DELAY      },
    { "AL_EAXREVERB_REFLECTIONS_PAN",         AL_EAXREVERB_REFLECTIONS_PAN        },
    { "AL_EAXREVERB_LATE_REVERB_GAIN",        AL_EAXREVERB_LATE_REVERB_GAIN       },
    { "AL_EAXREVERB_LATE_REVERB_DELAY",       AL_EAXREVERB_LATE_REVERB_DELAY      },
    { "AL_EAXREVERB_LATE_REVERB_PAN",         AL_EAXREVERB_LATE_REVERB_PAN        },
    { "AL_EAXREVERB_ECHO_TIME",               AL_EAXREVERB_ECHO_TIME              },
    { "AL_EAXREVERB_ECHO_DEPTH",              AL_EAXREVERB_ECHO_DEPTH             },
    { "AL_EAXREVERB_MODULATION_TIME",         AL_EAXREVERB_MODULATION_TIME        },
    { "AL_EAXREVERB_MODULATION_DEPTH",        AL_EAXREVERB_MODULATION_DEPTH       },
    { "AL_EAXREVERB_AIR_ABSORPTION_GAINHF",   AL_EAXREVERB_AIR_ABSORPTION_GAINHF  },
    { "AL_EAXREVERB_HFREFERENCE",             AL_EAXREVERB_HFREFERENCE            },
    { "AL_EAXREVERB_LFREFERENCE",             AL_EAXREVERB_LFREFERENCE            },
    { "AL_EAXREVERB_ROOM_ROLLOFF_FACTOR",     AL_EAXREVERB_ROOM_ROLLOFF_FACTOR    },
    { "AL_EAXREVERB_DECAY_HFLIMIT",           AL_EAXREVERB_DECAY_HFLIMIT          },

    { "AL_REVERB_DENSITY",                    AL_REVERB_DENSITY                   },
    { "AL_REVERB_DIFFUSION",                  AL_REVERB_DIFFUSION                 },
    { "AL_REVERB_GAIN",                       AL_REVERB_GAIN                      },
    { "AL_REVERB_GAINHF",                     AL_REVERB_GAINHF                    },
    { "AL_REVERB_DECAY_TIME",                 AL_REVERB_DECAY_TIME                },
    { "AL_REVERB_DECAY_HFRATIO",              AL_REVERB_DECAY_HFRATIO             },
    { "AL_REVERB_REFLECTIONS_GAIN",           AL_REVERB_REFLECTIONS_GAIN          },
    { "AL_REVERB_REFLECTIONS_DELAY",          AL_REVERB_REFLECTIONS_DELAY         },
    { "AL_REVERB_LATE_REVERB_GAIN",           AL_REVERB_LATE_REVERB_GAIN          },
    { "AL_REVERB_LATE_REVERB_DELAY",          AL_REVERB_LATE_REVERB_DELAY         },
    { "AL_REVERB_AIR_ABSORPTION_GAINHF",      AL_REVERB_AIR_ABSORPTION_GAINHF     },
    { "AL_REVERB_ROOM_ROLLOFF_FACTOR",        AL_REVERB_ROOM_ROLLOFF_FACTOR       },
    { "AL_REVERB_DECAY_HFLIMIT",              AL_REVERB_DECAY_HFLIMIT             },

    { "AL_ECHO_DELAY",                        AL_ECHO_DELAY                       },
    { "AL_ECHO_LRDELAY",                      AL_ECHO_LRDELAY                     },
    { "AL_ECHO_DAMPING",                      AL_ECHO_DAMPING                     },
    { "AL_ECHO_FEEDBACK",                     AL_ECHO_FEEDBACK                    },
    { "AL_ECHO_SPREAD",                       AL_ECHO_SPREAD                      },

    { "AL_RING_MODULATOR_FREQUENCY",          AL_RING_MODULATOR_FREQUENCY         },
    { "AL_RING_MODULATOR_HIGHPASS_CUTOFF",    AL_RING_MODULATOR_HIGHPASS_CUTOFF   },
    { "AL_RING_MODULATOR_WAVEFORM",           AL_RING_MODULATOR_WAVEFORM          },

    { "AL_DEDICATED_GAIN",                    AL_DEDICATED_GAIN                   },

    { NULL,                                   (ALCenum)0 }
};

static const ALCchar alcNoError[] = "No Error";
static const ALCchar alcErrInvalidDevice[] = "Invalid Device";
static const ALCchar alcErrInvalidContext[] = "Invalid Context";
static const ALCchar alcErrInvalidEnum[] = "Invalid Enum";
static const ALCchar alcErrInvalidValue[] = "Invalid Value";
static const ALCchar alcErrOutOfMemory[] = "Out of Memory";


/************************************************
 * Global variables
 ************************************************/

/* Enumerated device names */
static const ALCchar alcDefaultName[] = "OpenAL Soft\0";
static ALCchar *alcAllDeviceList;
static ALCchar *alcCaptureDeviceList;
/* Sizes only include the first ending null character, not the second */
static size_t alcAllDeviceListSize;
static size_t alcCaptureDeviceListSize;

/* Default is always the first in the list */
static ALCchar *alcDefaultAllDeviceSpecifier;
static ALCchar *alcCaptureDefaultDeviceSpecifier;

/* Default context extensions */
static const ALchar alExtList[] =
    "AL_EXT_ALAW AL_EXT_DOUBLE AL_EXT_EXPONENT_DISTANCE AL_EXT_FLOAT32 "
    "AL_EXT_IMA4 AL_EXT_LINEAR_DISTANCE AL_EXT_MCFORMATS AL_EXT_MULAW "
    "AL_EXT_MULAW_MCFORMATS AL_EXT_OFFSET AL_EXT_source_distance_model "
    "AL_LOKI_quadriphonic AL_SOFT_buffer_samples AL_SOFT_buffer_sub_data "
    "AL_SOFTX_deferred_updates AL_SOFT_direct_channels AL_SOFT_loop_points";

static volatile ALCenum LastNullDeviceError = ALC_NO_ERROR;

/* Thread-local current context */
static pthread_key_t LocalContext;
/* Process-wide current context */
static ALCcontext *volatile GlobalContext = NULL;

/* Mixing thread piority level */
ALint RTPrioLevel;

FILE *LogFile;
#ifdef _DEBUG
enum LogLevel LogLevel = LogWarning;
#else
enum LogLevel LogLevel = LogError;
#endif

/* Flag to trap ALC device errors */
static ALCboolean TrapALCError = ALC_FALSE;

/* One-time configuration init control */
static pthread_once_t alc_config_once = PTHREAD_ONCE_INIT;

/* Default effect that applies to sources that don't have an effect on send 0 */
static ALeffect DefaultEffect;


/************************************************
 * ALC information
 ************************************************/
static const ALCchar alcNoDeviceExtList[] =
    "ALC_ENUMERATE_ALL_EXT ALC_ENUMERATION_EXT ALC_EXT_CAPTURE "
    "ALC_EXT_thread_local_context ALC_SOFT_loopback";
static const ALCchar alcExtensionList[] =
    "ALC_ENUMERATE_ALL_EXT ALC_ENUMERATION_EXT ALC_EXT_CAPTURE "
    "ALC_EXT_DEDICATED ALC_EXT_disconnect ALC_EXT_EFX "
    "ALC_EXT_thread_local_context ALC_SOFT_loopback";
static const ALCint alcMajorVersion = 1;
static const ALCint alcMinorVersion = 1;

static const ALCint alcEFXMajorVersion = 1;
static const ALCint alcEFXMinorVersion = 0;


/************************************************
 * Device lists
 ************************************************/
static ALCdevice *volatile DeviceList = NULL;

static CRITICAL_SECTION ListLock;

static void LockLists(void)
{
    EnterCriticalSection(&ListLock);
}
static void UnlockLists(void)
{
    LeaveCriticalSection(&ListLock);
}

/************************************************
 * Library initialization
 ************************************************/
#if defined(_WIN32)
static void alc_init(void);
static void alc_deinit(void);
static void alc_deinit_safe(void);

UIntMap TlsDestructor;

#ifndef AL_LIBTYPE_STATIC
BOOL APIENTRY DllMain(HINSTANCE hModule,DWORD ul_reason_for_call,LPVOID lpReserved)
{
    ALsizei i;

    // Perform actions based on the reason for calling.
    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            /* Pin the DLL so we won't get unloaded until the process terminates */
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                               (WCHAR*)hModule, &hModule);
            InitUIntMap(&TlsDestructor, ~0);
            alc_init();
            break;

        case DLL_THREAD_DETACH:
            LockUIntMapRead(&TlsDestructor);
            for(i = 0;i < TlsDestructor.size;i++)
            {
                void *ptr = pthread_getspecific(TlsDestructor.array[i].key);
                void (*callback)(void*) = (void(*)(void*))TlsDestructor.array[i].value;
                if(ptr && callback)
                    callback(ptr);
            }
            UnlockUIntMapRead(&TlsDestructor);
            break;

        case DLL_PROCESS_DETACH:
            if(!lpReserved)
                alc_deinit();
            else
                alc_deinit_safe();
            ResetUIntMap(&TlsDestructor);
            break;
    }
    return TRUE;
}
#elif defined(_MSC_VER)
#pragma section(".CRT$XCU",read)
static void alc_constructor(void);
static void alc_destructor(void);
__declspec(allocate(".CRT$XCU")) void (__cdecl* alc_constructor_)(void) = alc_constructor;

static void alc_constructor(void)
{
    atexit(alc_destructor);
    alc_init();
}

static void alc_destructor(void)
{
    alc_deinit();
}
#elif defined(HAVE_GCC_DESTRUCTOR)
static void alc_init(void) __attribute__((constructor));
static void alc_deinit(void) __attribute__((destructor));
#else
#error "No static initialization available on this platform!"
#endif

#elif defined(HAVE_GCC_DESTRUCTOR)

static void alc_init(void) __attribute__((constructor));
static void alc_deinit(void) __attribute__((destructor));

#else
#error "No global initialization available on this platform!"
#endif

static void ReleaseThreadCtx(void *ptr);
static void alc_init(void)
{
    const char *str;

    LogFile = stderr;

    str = getenv("__ALSOFT_HALF_ANGLE_CONES");
    if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
        ConeScale = 1.0f;

    str = getenv("__ALSOFT_REVERSE_Z");
    if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
        ZScale = -1.0f;

    pthread_key_create(&LocalContext, ReleaseThreadCtx);
    InitializeCriticalSection(&ListLock);
    ThunkInit();
}

static void alc_initconfig(void)
{
    const char *devs, *str;
    float valf;
    int i, n;

    str = getenv("ALSOFT_LOGLEVEL");
    if(str)
    {
        long lvl = strtol(str, NULL, 0);
        if(lvl >= NoLog && lvl <= LogRef)
            LogLevel = lvl;
    }

    str = getenv("ALSOFT_LOGFILE");
    if(str && str[0])
    {
        FILE *logfile = fopen(str, "wat");
        if(logfile) LogFile = logfile;
        else ERR("Failed to open log file '%s'\n", str);
    }

    ReadALConfig();

    InitHrtf();

#ifdef _WIN32
    RTPrioLevel = 1;
#else
    RTPrioLevel = 0;
#endif
    ConfigValueInt(NULL, "rt-prio", &RTPrioLevel);

    if(ConfigValueStr(NULL, "resampler", &str))
    {
        if(strcasecmp(str, "point") == 0 || strcasecmp(str, "none") == 0)
            DefaultResampler = PointResampler;
        else if(strcasecmp(str, "linear") == 0)
            DefaultResampler = LinearResampler;
        else if(strcasecmp(str, "cubic") == 0)
            DefaultResampler = CubicResampler;
        else
        {
            char *end;

            n = strtol(str, &end, 0);
            if(*end == '\0' && (n == PointResampler || n == LinearResampler || n == CubicResampler))
                DefaultResampler = n;
            else
                WARN("Invalid resampler: %s\n", str);
        }
    }

    str = getenv("ALSOFT_TRAP_ERROR");
    if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
    {
        TrapALError  = AL_TRUE;
        TrapALCError = AL_TRUE;
    }
    else
    {
        str = getenv("ALSOFT_TRAP_AL_ERROR");
        if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
            TrapALError = AL_TRUE;
        TrapALError = GetConfigValueBool(NULL, "trap-al-error", TrapALError);

        str = getenv("ALSOFT_TRAP_ALC_ERROR");
        if(str && (strcasecmp(str, "true") == 0 || strtol(str, NULL, 0) == 1))
            TrapALCError = ALC_TRUE;
        TrapALCError = GetConfigValueBool(NULL, "trap-alc-error", TrapALCError);
    }

    if(ConfigValueFloat("reverb", "boost", &valf))
        ReverbBoost *= aluPow(10.0f, valf / 20.0f);

    EmulateEAXReverb = GetConfigValueBool("reverb", "emulate-eax", AL_FALSE);

    if(((devs=getenv("ALSOFT_DRIVERS")) && devs[0]) ||
       ConfigValueStr(NULL, "drivers", &devs))
    {
        int n;
        size_t len;
        const char *next = devs;
        int endlist, delitem;

        i = 0;
        do {
            devs = next;
            next = strchr(devs, ',');

            delitem = (devs[0] == '-');
            if(devs[0] == '-') devs++;

            if(!devs[0] || devs[0] == ',')
            {
                endlist = 0;
                continue;
            }
            endlist = 1;

            len = (next ? ((size_t)(next-devs)) : strlen(devs));
            for(n = i;BackendList[n].Init;n++)
            {
                if(len == strlen(BackendList[n].name) &&
                   strncmp(BackendList[n].name, devs, len) == 0)
                {
                    if(delitem)
                    {
                        do {
                            BackendList[n] = BackendList[n+1];
                            ++n;
                        } while(BackendList[n].Init);
                    }
                    else
                    {
                        struct BackendInfo Bkp = BackendList[n];
                        while(n > i)
                        {
                            BackendList[n] = BackendList[n-1];
                            --n;
                        }
                        BackendList[n] = Bkp;

                        i++;
                    }
                    break;
                }
            }
        } while(next++);

        if(endlist)
        {
            BackendList[i].name = NULL;
            BackendList[i].Init = NULL;
            BackendList[i].Deinit = NULL;
            BackendList[i].Probe = NULL;
        }
    }

    for(i = 0;BackendList[i].Init && (!PlaybackBackend.name || !CaptureBackend.name);i++)
    {
        if(!BackendList[i].Init(&BackendList[i].Funcs))
        {
            WARN("Failed to initialize backend \"%s\"\n", BackendList[i].name);
            continue;
        }

        TRACE("Initialized backend \"%s\"\n", BackendList[i].name);
        if(BackendList[i].Funcs.OpenPlayback && !PlaybackBackend.name)
        {
            PlaybackBackend = BackendList[i];
            TRACE("Added \"%s\" for playback\n", PlaybackBackend.name);
        }
        if(BackendList[i].Funcs.OpenCapture && !CaptureBackend.name)
        {
            CaptureBackend = BackendList[i];
            TRACE("Added \"%s\" for capture\n", CaptureBackend.name);
        }
    }
    BackendLoopback.Init(&BackendLoopback.Funcs);

    if(ConfigValueStr(NULL, "excludefx", &str))
    {
        size_t len;
        const char *next = str;

        do {
            str = next;
            next = strchr(str, ',');

            if(!str[0] || next == str)
                continue;

            len = (next ? ((size_t)(next-str)) : strlen(str));
            for(n = 0;EffectList[n].name;n++)
            {
                if(len == strlen(EffectList[n].name) &&
                   strncmp(EffectList[n].name, str, len) == 0)
                    DisabledEffects[EffectList[n].type] = AL_TRUE;
            }
        } while(next++);
    }

    InitEffect(&DefaultEffect);
    str = getenv("ALSOFT_DEFAULT_REVERB");
    if((str && str[0]) || ConfigValueStr(NULL, "default-reverb", &str))
        LoadReverbPreset(str, &DefaultEffect);
}
#define DO_INITCONFIG() pthread_once(&alc_config_once, alc_initconfig)


/************************************************
 * Library deinitialization
 ************************************************/
static void alc_cleanup(void)
{
    ALCdevice *dev;

    free(alcAllDeviceList); alcAllDeviceList = NULL;
    alcAllDeviceListSize = 0;
    free(alcCaptureDeviceList); alcCaptureDeviceList = NULL;
    alcCaptureDeviceListSize = 0;

    free(alcDefaultAllDeviceSpecifier);
    alcDefaultAllDeviceSpecifier = NULL;
    free(alcCaptureDefaultDeviceSpecifier);
    alcCaptureDefaultDeviceSpecifier = NULL;

    if((dev=ExchangePtr((XchgPtr*)&DeviceList, NULL)) != NULL)
    {
        ALCuint num = 0;
        do {
            num++;
        } while((dev=dev->next) != NULL);
        ERR("%u device%s not closed\n", num, (num>1)?"s":"");
    }
}

static void alc_deinit_safe(void)
{
    alc_cleanup();

    FreeHrtf();
    FreeALConfig();

    ThunkExit();
    DeleteCriticalSection(&ListLock);
    pthread_key_delete(LocalContext);

    if(LogFile != stderr)
        fclose(LogFile);
    LogFile = NULL;
}

static void alc_deinit(void)
{
    int i;

    alc_cleanup();

    memset(&PlaybackBackend, 0, sizeof(PlaybackBackend));
    memset(&CaptureBackend, 0, sizeof(CaptureBackend));

    for(i = 0;BackendList[i].Deinit;i++)
        BackendList[i].Deinit();
    BackendLoopback.Deinit();

    alc_deinit_safe();
}


/************************************************
 * Device enumeration
 ************************************************/
static void ProbeList(ALCchar **list, size_t *listsize, enum DevProbe type)
{
    DO_INITCONFIG();

    LockLists();
    free(*list);
    *list = NULL;
    *listsize = 0;

    if(type == ALL_DEVICE_PROBE && PlaybackBackend.Probe)
        PlaybackBackend.Probe(type);
    else if(type == CAPTURE_DEVICE_PROBE && CaptureBackend.Probe)
        CaptureBackend.Probe(type);
    UnlockLists();
}

static void ProbeAllDeviceList(void)
{ ProbeList(&alcAllDeviceList, &alcAllDeviceListSize, ALL_DEVICE_PROBE); }
static void ProbeCaptureDeviceList(void)
{ ProbeList(&alcCaptureDeviceList, &alcCaptureDeviceListSize, CAPTURE_DEVICE_PROBE); }


static void AppendList(const ALCchar *name, ALCchar **List, size_t *ListSize)
{
    size_t len = strlen(name);
    void *temp;

    if(len == 0)
        return;

    temp = realloc(*List, (*ListSize) + len + 2);
    if(!temp)
    {
        ERR("Realloc failed to add %s!\n", name);
        return;
    }
    *List = temp;

    memcpy((*List)+(*ListSize), name, len+1);
    *ListSize += len+1;
    (*List)[*ListSize] = 0;
}

#define DECL_APPEND_LIST_FUNC(type)                                          \
void Append##type##List(const ALCchar *name)                                 \
{ AppendList(name, &alc##type##List, &alc##type##ListSize); }

DECL_APPEND_LIST_FUNC(AllDevice)
DECL_APPEND_LIST_FUNC(CaptureDevice)

#undef DECL_APPEND_LIST_FUNC


/************************************************
 * Device format information
 ************************************************/
const ALCchar *DevFmtTypeString(enum DevFmtType type)
{
    switch(type)
    {
    case DevFmtByte: return "Signed Byte";
    case DevFmtUByte: return "Unsigned Byte";
    case DevFmtShort: return "Signed Short";
    case DevFmtUShort: return "Unsigned Short";
    case DevFmtInt: return "Signed Int";
    case DevFmtUInt: return "Unsigned Int";
    case DevFmtFloat: return "Float";
    }
    return "(unknown type)";
}
const ALCchar *DevFmtChannelsString(enum DevFmtChannels chans)
{
    switch(chans)
    {
    case DevFmtMono: return "Mono";
    case DevFmtStereo: return "Stereo";
    case DevFmtQuad: return "Quadraphonic";
    case DevFmtX51: return "5.1 Surround";
    case DevFmtX51Side: return "5.1 Side";
    case DevFmtX61: return "6.1 Surround";
    case DevFmtX71: return "7.1 Surround";
    }
    return "(unknown channels)";
}

ALuint BytesFromDevFmt(enum DevFmtType type)
{
    switch(type)
    {
    case DevFmtByte: return sizeof(ALbyte);
    case DevFmtUByte: return sizeof(ALubyte);
    case DevFmtShort: return sizeof(ALshort);
    case DevFmtUShort: return sizeof(ALushort);
    case DevFmtInt: return sizeof(ALint);
    case DevFmtUInt: return sizeof(ALuint);
    case DevFmtFloat: return sizeof(ALfloat);
    }
    return 0;
}
ALuint ChannelsFromDevFmt(enum DevFmtChannels chans)
{
    switch(chans)
    {
    case DevFmtMono: return 1;
    case DevFmtStereo: return 2;
    case DevFmtQuad: return 4;
    case DevFmtX51: return 6;
    case DevFmtX51Side: return 6;
    case DevFmtX61: return 7;
    case DevFmtX71: return 8;
    }
    return 0;
}

static ALboolean DecomposeDevFormat(ALenum format, enum DevFmtChannels *chans,
                                    enum DevFmtType *type)
{
    static const struct {
        ALenum format;
        enum DevFmtChannels channels;
        enum DevFmtType type;
    } list[] = {
        { AL_FORMAT_MONO8,        DevFmtMono, DevFmtUByte },
        { AL_FORMAT_MONO16,       DevFmtMono, DevFmtShort },
        { AL_FORMAT_MONO_FLOAT32, DevFmtMono, DevFmtFloat },

        { AL_FORMAT_STEREO8,        DevFmtStereo, DevFmtUByte },
        { AL_FORMAT_STEREO16,       DevFmtStereo, DevFmtShort },
        { AL_FORMAT_STEREO_FLOAT32, DevFmtStereo, DevFmtFloat },

        { AL_FORMAT_QUAD8,  DevFmtQuad, DevFmtUByte },
        { AL_FORMAT_QUAD16, DevFmtQuad, DevFmtShort },
        { AL_FORMAT_QUAD32, DevFmtQuad, DevFmtFloat },

        { AL_FORMAT_51CHN8,  DevFmtX51, DevFmtUByte },
        { AL_FORMAT_51CHN16, DevFmtX51, DevFmtShort },
        { AL_FORMAT_51CHN32, DevFmtX51, DevFmtFloat },

        { AL_FORMAT_61CHN8,  DevFmtX61, DevFmtUByte },
        { AL_FORMAT_61CHN16, DevFmtX61, DevFmtShort },
        { AL_FORMAT_61CHN32, DevFmtX61, DevFmtFloat },

        { AL_FORMAT_71CHN8,  DevFmtX71, DevFmtUByte },
        { AL_FORMAT_71CHN16, DevFmtX71, DevFmtShort },
        { AL_FORMAT_71CHN32, DevFmtX71, DevFmtFloat },
    };
    ALuint i;

    for(i = 0;i < COUNTOF(list);i++)
    {
        if(list[i].format == format)
        {
            *chans = list[i].channels;
            *type  = list[i].type;
            return AL_TRUE;
        }
    }

    return AL_FALSE;
}

static ALCboolean IsValidALCType(ALCenum type)
{
    switch(type)
    {
        case ALC_BYTE_SOFT:
        case ALC_UNSIGNED_BYTE_SOFT:
        case ALC_SHORT_SOFT:
        case ALC_UNSIGNED_SHORT_SOFT:
        case ALC_INT_SOFT:
        case ALC_UNSIGNED_INT_SOFT:
        case ALC_FLOAT_SOFT:
            return ALC_TRUE;
    }
    return ALC_FALSE;
}

static ALCboolean IsValidALCChannels(ALCenum channels)
{
    switch(channels)
    {
        case ALC_MONO_SOFT:
        case ALC_STEREO_SOFT:
        case ALC_QUAD_SOFT:
        case ALC_5POINT1_SOFT:
        case ALC_6POINT1_SOFT:
        case ALC_7POINT1_SOFT:
            return ALC_TRUE;
    }
    return ALC_FALSE;
}


/************************************************
 * Miscellaneous ALC helpers
 ************************************************/

/* SetDefaultWFXChannelOrder
 *
 * Sets the default channel order used by WaveFormatEx.
 */
void SetDefaultWFXChannelOrder(ALCdevice *device)
{
    switch(device->FmtChans)
    {
    case DevFmtMono: device->DevChannels[0] = FRONT_CENTER; break;

    case DevFmtStereo: device->DevChannels[0] = FRONT_LEFT;
                       device->DevChannels[1] = FRONT_RIGHT; break;

    case DevFmtQuad: device->DevChannels[0] = FRONT_LEFT;
                     device->DevChannels[1] = FRONT_RIGHT;
                     device->DevChannels[2] = BACK_LEFT;
                     device->DevChannels[3] = BACK_RIGHT; break;

    case DevFmtX51: device->DevChannels[0] = FRONT_LEFT;
                    device->DevChannels[1] = FRONT_RIGHT;
                    device->DevChannels[2] = FRONT_CENTER;
                    device->DevChannels[3] = LFE;
                    device->DevChannels[4] = BACK_LEFT;
                    device->DevChannels[5] = BACK_RIGHT; break;

    case DevFmtX51Side: device->DevChannels[0] = FRONT_LEFT;
                        device->DevChannels[1] = FRONT_RIGHT;
                        device->DevChannels[2] = FRONT_CENTER;
                        device->DevChannels[3] = LFE;
                        device->DevChannels[4] = SIDE_LEFT;
                        device->DevChannels[5] = SIDE_RIGHT; break;

    case DevFmtX61: device->DevChannels[0] = FRONT_LEFT;
                    device->DevChannels[1] = FRONT_RIGHT;
                    device->DevChannels[2] = FRONT_CENTER;
                    device->DevChannels[3] = LFE;
                    device->DevChannels[4] = BACK_CENTER;
                    device->DevChannels[5] = SIDE_LEFT;
                    device->DevChannels[6] = SIDE_RIGHT; break;

    case DevFmtX71: device->DevChannels[0] = FRONT_LEFT;
                    device->DevChannels[1] = FRONT_RIGHT;
                    device->DevChannels[2] = FRONT_CENTER;
                    device->DevChannels[3] = LFE;
                    device->DevChannels[4] = BACK_LEFT;
                    device->DevChannels[5] = BACK_RIGHT;
                    device->DevChannels[6] = SIDE_LEFT;
                    device->DevChannels[7] = SIDE_RIGHT; break;
    }
}

/* SetDefaultChannelOrder
 *
 * Sets the default channel order used by most non-WaveFormatEx-based APIs.
 */
void SetDefaultChannelOrder(ALCdevice *device)
{
    switch(device->FmtChans)
    {
    case DevFmtX51: device->DevChannels[0] = FRONT_LEFT;
                    device->DevChannels[1] = FRONT_RIGHT;
                    device->DevChannels[2] = BACK_LEFT;
                    device->DevChannels[3] = BACK_RIGHT;
                    device->DevChannels[4] = FRONT_CENTER;
                    device->DevChannels[5] = LFE;
                    return;

    case DevFmtX71: device->DevChannels[0] = FRONT_LEFT;
                    device->DevChannels[1] = FRONT_RIGHT;
                    device->DevChannels[2] = BACK_LEFT;
                    device->DevChannels[3] = BACK_RIGHT;
                    device->DevChannels[4] = FRONT_CENTER;
                    device->DevChannels[5] = LFE;
                    device->DevChannels[6] = SIDE_LEFT;
                    device->DevChannels[7] = SIDE_RIGHT;
                    return;

    /* Same as WFX order */
    case DevFmtMono:
    case DevFmtStereo:
    case DevFmtQuad:
    case DevFmtX51Side:
    case DevFmtX61:
        break;
    }
    SetDefaultWFXChannelOrder(device);
}


/* alcSetError
 *
 * Stores the latest ALC device error
 */
static void alcSetError(ALCdevice *device, ALCenum errorCode)
{
    if(TrapALCError)
    {
#ifdef _WIN32
        /* DebugBreak() will cause an exception if there is no debugger */
        if(IsDebuggerPresent())
            DebugBreak();
#elif defined(SIGTRAP)
        raise(SIGTRAP);
#endif
    }

    if(device)
        device->LastError = errorCode;
    else
        LastNullDeviceError = errorCode;
}


/* UpdateDeviceParams
 *
 * Updates device parameters according to the attribute list (caller is
 * responsible for holding the list lock).
 */
static ALCenum UpdateDeviceParams(ALCdevice *device, const ALCint *attrList)
{
    ALCcontext *context;
    enum DevFmtChannels oldChans;
    enum DevFmtType oldType;
    ALCuint oldFreq;
    int oldMode;
    ALuint i;

    // Check for attributes
    if(device->Type == Loopback)
    {
        enum {
            GotFreq  = 1<<0,
            GotChans = 1<<1,
            GotType  = 1<<2,
            GotAll   = GotFreq|GotChans|GotType
        };
        ALCuint freq, numMono, numStereo, numSends;
        enum DevFmtChannels schans;
        enum DevFmtType stype;
        ALCuint attrIdx = 0;
        ALCint gotFmt = 0;

        if(!attrList)
        {
            WARN("Missing attributes for loopback device\n");
            return ALC_INVALID_VALUE;
        }

        numMono = device->NumMonoSources;
        numStereo = device->NumStereoSources;
        numSends = device->NumAuxSends;
        schans = device->FmtChans;
        stype = device->FmtType;
        freq = device->Frequency;

        while(attrList[attrIdx])
        {
            if(attrList[attrIdx] == ALC_FORMAT_CHANNELS_SOFT)
            {
                ALCint val = attrList[attrIdx + 1];
                if(!IsValidALCChannels(val) || !ChannelsFromDevFmt(val))
                    return ALC_INVALID_VALUE;
                schans = val;
                gotFmt |= GotChans;
            }

            if(attrList[attrIdx] == ALC_FORMAT_TYPE_SOFT)
            {
                ALCint val = attrList[attrIdx + 1];
                if(!IsValidALCType(val) || !BytesFromDevFmt(val))
                    return ALC_INVALID_VALUE;
                stype = val;
                gotFmt |= GotType;
            }

            if(attrList[attrIdx] == ALC_FREQUENCY)
            {
                freq = attrList[attrIdx + 1];
                if(freq < MIN_OUTPUT_RATE)
                    return ALC_INVALID_VALUE;
                gotFmt |= GotFreq;
            }

            if(attrList[attrIdx] == ALC_STEREO_SOURCES)
            {
                numStereo = attrList[attrIdx + 1];
                if(numStereo > device->MaxNoOfSources)
                    numStereo = device->MaxNoOfSources;

                numMono = device->MaxNoOfSources - numStereo;
            }

            if(attrList[attrIdx] == ALC_MAX_AUXILIARY_SENDS)
                numSends = attrList[attrIdx + 1];

            attrIdx += 2;
        }

        if(gotFmt != GotAll)
        {
            WARN("Missing format for loopback device\n");
            return ALC_INVALID_VALUE;
        }

        ConfigValueUInt(NULL, "sends", &numSends);
        numSends = minu(MAX_SENDS, numSends);

        if((device->Flags&DEVICE_RUNNING))
            ALCdevice_StopPlayback(device);
        device->Flags &= ~DEVICE_RUNNING;

        device->Frequency = freq;
        device->FmtChans = schans;
        device->FmtType = stype;
        device->NumMonoSources = numMono;
        device->NumStereoSources = numStereo;
        device->NumAuxSends = numSends;
    }
    else if(attrList && attrList[0])
    {
        ALCuint freq, numMono, numStereo, numSends;
        ALCuint attrIdx = 0;

        /* If a context is already running on the device, stop playback so the
         * device attributes can be updated. */
        if((device->Flags&DEVICE_RUNNING))
            ALCdevice_StopPlayback(device);
        device->Flags &= ~DEVICE_RUNNING;

        freq = device->Frequency;
        numMono = device->NumMonoSources;
        numStereo = device->NumStereoSources;
        numSends = device->NumAuxSends;

        while(attrList[attrIdx])
        {
            if(attrList[attrIdx] == ALC_FREQUENCY)
            {
                freq = attrList[attrIdx + 1];
                device->Flags |= DEVICE_FREQUENCY_REQUEST;
            }

            if(attrList[attrIdx] == ALC_STEREO_SOURCES)
            {
                numStereo = attrList[attrIdx + 1];
                if(numStereo > device->MaxNoOfSources)
                    numStereo = device->MaxNoOfSources;

                numMono = device->MaxNoOfSources - numStereo;
            }

            if(attrList[attrIdx] == ALC_MAX_AUXILIARY_SENDS)
                numSends = attrList[attrIdx + 1];

            attrIdx += 2;
        }

        ConfigValueUInt(NULL, "frequency", &freq);
        freq = maxu(freq, MIN_OUTPUT_RATE);

        ConfigValueUInt(NULL, "sends", &numSends);
        numSends = minu(MAX_SENDS, numSends);

        device->UpdateSize = (ALuint64)device->UpdateSize * freq /
                             device->Frequency;

        device->Frequency = freq;
        device->NumMonoSources = numMono;
        device->NumStereoSources = numStereo;
        device->NumAuxSends = numSends;
    }

    if((device->Flags&DEVICE_RUNNING))
        return ALC_NO_ERROR;

    oldFreq  = device->Frequency;
    oldChans = device->FmtChans;
    oldType  = device->FmtType;

    TRACE("Format pre-setup: %s%s, %s%s, %uhz%s, %u update size x%d\n",
          DevFmtChannelsString(device->FmtChans),
          (device->Flags&DEVICE_CHANNELS_REQUEST)?" (requested)":"",
          DevFmtTypeString(device->FmtType),
          (device->Flags&DEVICE_SAMPLE_TYPE_REQUEST)?" (requested)":"",
          device->Frequency,
          (device->Flags&DEVICE_FREQUENCY_REQUEST)?" (requested)":"",
          device->UpdateSize, device->NumUpdates);

    if(ALCdevice_ResetPlayback(device) == ALC_FALSE)
        return ALC_INVALID_DEVICE;

    if(device->FmtChans != oldChans && (device->Flags&DEVICE_CHANNELS_REQUEST))
    {
        ERR("Failed to set %s, got %s instead\n", DevFmtChannelsString(oldChans),
            DevFmtChannelsString(device->FmtChans));
        device->Flags &= ~DEVICE_CHANNELS_REQUEST;
    }
    if(device->FmtType != oldType && (device->Flags&DEVICE_SAMPLE_TYPE_REQUEST))
    {
        ERR("Failed to set %s, got %s instead\n", DevFmtTypeString(oldType),
            DevFmtTypeString(device->FmtType));
        device->Flags &= ~DEVICE_SAMPLE_TYPE_REQUEST;
    }
    if(device->Frequency != oldFreq && (device->Flags&DEVICE_FREQUENCY_REQUEST))
    {
        ERR("Failed to set %uhz, got %uhz instead\n", oldFreq, device->Frequency);
        device->Flags &= ~DEVICE_FREQUENCY_REQUEST;
    }

    TRACE("Format post-setup: %s, %s, %uhz, %u update size x%d\n",
          DevFmtChannelsString(device->FmtChans),
          DevFmtTypeString(device->FmtType), device->Frequency,
          device->UpdateSize, device->NumUpdates);

    aluInitPanning(device);

    for(i = 0;i < MAXCHANNELS;i++)
    {
        device->ClickRemoval[i] = 0.0f;
        device->PendingClicks[i] = 0.0f;
    }

    device->Hrtf = NULL;
    if(device->Type != Loopback && GetConfigValueBool(NULL, "hrtf", AL_FALSE))
        device->Hrtf = GetHrtf(device);
    TRACE("HRTF %s\n", device->Hrtf?"enabled":"disabled");

    if(!device->Hrtf && device->Bs2bLevel > 0 && device->Bs2bLevel <= 6)
    {
        if(!device->Bs2b)
        {
            device->Bs2b = calloc(1, sizeof(*device->Bs2b));
            bs2b_clear(device->Bs2b);
        }
        bs2b_set_srate(device->Bs2b, device->Frequency);
        bs2b_set_level(device->Bs2b, device->Bs2bLevel);
        TRACE("BS2B level %d\n", device->Bs2bLevel);
    }
    else
    {
        free(device->Bs2b);
        device->Bs2b = NULL;
        TRACE("BS2B disabled\n");
    }

    device->Flags &= ~DEVICE_DUPLICATE_STEREO;
    switch(device->FmtChans)
    {
        case DevFmtMono:
        case DevFmtStereo:
            break;
        case DevFmtQuad:
        case DevFmtX51:
        case DevFmtX51Side:
        case DevFmtX61:
        case DevFmtX71:
            if(GetConfigValueBool(NULL, "stereodup", AL_TRUE))
                device->Flags |= DEVICE_DUPLICATE_STEREO;
            break;
    }
    TRACE("Stereo duplication %s\n", (device->Flags&DEVICE_DUPLICATE_STEREO)?"enabled":"disabled");

    oldMode = SetMixerFPUMode();
    LockDevice(device);
    context = device->ContextList;
    while(context)
    {
        ALsizei pos;

        context->UpdateSources = AL_FALSE;
        LockUIntMapRead(&context->EffectSlotMap);
        for(pos = 0;pos < context->EffectSlotMap.size;pos++)
        {
            ALeffectslot *slot = context->EffectSlotMap.array[pos].value;

            if(ALeffectState_DeviceUpdate(slot->EffectState, device) == AL_FALSE)
            {
                UnlockUIntMapRead(&context->EffectSlotMap);
                UnlockDevice(device);
                RestoreFPUMode(oldMode);
                return ALC_INVALID_DEVICE;
            }
            slot->NeedsUpdate = AL_FALSE;
            ALeffectState_Update(slot->EffectState, device, slot);
        }
        UnlockUIntMapRead(&context->EffectSlotMap);

        LockUIntMapRead(&context->SourceMap);
        for(pos = 0;pos < context->SourceMap.size;pos++)
        {
            ALsource *source = context->SourceMap.array[pos].value;
            ALuint s = device->NumAuxSends;
            while(s < MAX_SENDS)
            {
                if(source->Send[s].Slot)
                    DecrementRef(&source->Send[s].Slot->ref);
                source->Send[s].Slot = NULL;
                source->Send[s].WetGain = 1.0f;
                source->Send[s].WetGainHF = 1.0f;
                s++;
            }
            source->NeedsUpdate = AL_FALSE;
            ALsource_Update(source, context);
        }
        UnlockUIntMapRead(&context->SourceMap);

        context = context->next;
    }
    if(device->DefaultSlot)
    {
        ALeffectslot *slot = device->DefaultSlot;

        if(ALeffectState_DeviceUpdate(slot->EffectState, device) == AL_FALSE)
        {
            UnlockDevice(device);
            RestoreFPUMode(oldMode);
            return ALC_INVALID_DEVICE;
        }
        slot->NeedsUpdate = AL_FALSE;
        ALeffectState_Update(slot->EffectState, device, slot);
    }
    UnlockDevice(device);
    RestoreFPUMode(oldMode);

    if(ALCdevice_StartPlayback(device) == ALC_FALSE)
        return ALC_INVALID_DEVICE;
    device->Flags |= DEVICE_RUNNING;

    return ALC_NO_ERROR;
}

/* FreeDevice
 *
 * Frees the device structure, and destroys any objects the app failed to
 * delete. Called once there's no more references on the device.
 */
static ALCvoid FreeDevice(ALCdevice *device)
{
    TRACE("%p\n", device);

    if(device->DefaultSlot)
    {
        ALeffectState_Destroy(device->DefaultSlot->EffectState);
        device->DefaultSlot->EffectState = NULL;
    }

    if(device->BufferMap.size > 0)
    {
        WARN("(%p) Deleting %d Buffer(s)\n", device, device->BufferMap.size);
        ReleaseALBuffers(device);
    }
    ResetUIntMap(&device->BufferMap);

    if(device->EffectMap.size > 0)
    {
        WARN("(%p) Deleting %d Effect(s)\n", device, device->EffectMap.size);
        ReleaseALEffects(device);
    }
    ResetUIntMap(&device->EffectMap);

    if(device->FilterMap.size > 0)
    {
        WARN("(%p) Deleting %d Filter(s)\n", device, device->FilterMap.size);
        ReleaseALFilters(device);
    }
    ResetUIntMap(&device->FilterMap);

    free(device->Bs2b);
    device->Bs2b = NULL;

    free(device->DeviceName);
    device->DeviceName = NULL;

    DeleteCriticalSection(&device->Mutex);

    free(device);
}


void ALCdevice_IncRef(ALCdevice *device)
{
    RefCount ref;
    ref = IncrementRef(&device->ref);
    TRACEREF("%p increasing refcount to %u\n", device, ref);
}

void ALCdevice_DecRef(ALCdevice *device)
{
    RefCount ref;
    ref = DecrementRef(&device->ref);
    TRACEREF("%p decreasing refcount to %u\n", device, ref);
    if(ref == 0) FreeDevice(device);
}

/* VerifyDevice
 *
 * Checks if the device handle is valid, and increments its ref count if so.
 */
static ALCdevice *VerifyDevice(ALCdevice *device)
{
    ALCdevice *tmpDevice;

    if(!device)
        return NULL;

    LockLists();
    tmpDevice = DeviceList;
    while(tmpDevice && tmpDevice != device)
        tmpDevice = tmpDevice->next;

    if(tmpDevice)
        ALCdevice_IncRef(tmpDevice);
    UnlockLists();
    return tmpDevice;
}


/* InitContext
 *
 * Initializes context fields
 */
static ALvoid InitContext(ALCcontext *Context)
{
    ALint i, j;

    //Initialise listener
    Context->Listener.Gain = 1.0f;
    Context->Listener.MetersPerUnit = 1.0f;
    Context->Listener.Position[0] = 0.0f;
    Context->Listener.Position[1] = 0.0f;
    Context->Listener.Position[2] = 0.0f;
    Context->Listener.Velocity[0] = 0.0f;
    Context->Listener.Velocity[1] = 0.0f;
    Context->Listener.Velocity[2] = 0.0f;
    Context->Listener.Forward[0] = 0.0f;
    Context->Listener.Forward[1] = 0.0f;
    Context->Listener.Forward[2] = -1.0f;
    Context->Listener.Up[0] = 0.0f;
    Context->Listener.Up[1] = 1.0f;
    Context->Listener.Up[2] = 0.0f;
    for(i = 0;i < 4;i++)
    {
        for(j = 0;j < 4;j++)
            Context->Listener.Matrix[i][j] = ((i==j) ? 1.0f : 0.0f);
    }

    //Validate Context
    Context->LastError = AL_NO_ERROR;
    Context->UpdateSources = AL_FALSE;
    Context->ActiveSourceCount = 0;
    InitUIntMap(&Context->SourceMap, Context->Device->MaxNoOfSources);
    InitUIntMap(&Context->EffectSlotMap, Context->Device->AuxiliaryEffectSlotMax);

    //Set globals
    Context->DistanceModel = AL_INVERSE_DISTANCE_CLAMPED;
    Context->SourceDistanceModel = AL_FALSE;
    Context->DopplerFactor = 1.0f;
    Context->DopplerVelocity = 1.0f;
    Context->SpeedOfSound = SPEEDOFSOUNDMETRESPERSEC;
    Context->DeferUpdates = AL_FALSE;

    Context->ExtensionList = alExtList;
}


/* FreeContext
 *
 * Cleans up the context, and destroys any remaining objects the app failed to
 * delete. Called once there's no more references on the context.
 */
static ALCvoid FreeContext(ALCcontext *context)
{
    TRACE("%p\n", context);

    if(context->SourceMap.size > 0)
    {
        ERR("(%p) Deleting %d Source(s)\n", context, context->SourceMap.size);
        ReleaseALSources(context);
    }
    ResetUIntMap(&context->SourceMap);

    if(context->EffectSlotMap.size > 0)
    {
        ERR("(%p) Deleting %d AuxiliaryEffectSlot(s)\n", context, context->EffectSlotMap.size);
        ReleaseALAuxiliaryEffectSlots(context);
    }
    ResetUIntMap(&context->EffectSlotMap);

    context->ActiveSourceCount = 0;
    free(context->ActiveSources);
    context->ActiveSources = NULL;
    context->MaxActiveSources = 0;

    context->ActiveEffectSlotCount = 0;
    free(context->ActiveEffectSlots);
    context->ActiveEffectSlots = NULL;
    context->MaxActiveEffectSlots = 0;

    ALCdevice_DecRef(context->Device);
    context->Device = NULL;

    //Invalidate context
    memset(context, 0, sizeof(ALCcontext));
    free(context);
}

/* ReleaseContext
 *
 * Removes the context reference from the given device and removes it from
 * being current on the running thread or globally.
 */
static void ReleaseContext(ALCcontext *context, ALCdevice *device)
{
    ALCcontext *volatile*tmp_ctx;

    if(pthread_getspecific(LocalContext) == context)
    {
        WARN("%p released while current on thread\n", context);
        pthread_setspecific(LocalContext, NULL);
        ALCcontext_DecRef(context);
    }

    if(CompExchangePtr((XchgPtr*)&GlobalContext, context, NULL))
        ALCcontext_DecRef(context);

    LockDevice(device);
    tmp_ctx = &device->ContextList;
    while(*tmp_ctx)
    {
        if(CompExchangePtr((XchgPtr*)tmp_ctx, context, context->next))
            break;
        tmp_ctx = &(*tmp_ctx)->next;
    }
    UnlockDevice(device);

    ALCcontext_DecRef(context);
}

void ALCcontext_IncRef(ALCcontext *context)
{
    RefCount ref;
    ref = IncrementRef(&context->ref);
    TRACEREF("%p increasing refcount to %u\n", context, ref);
}

void ALCcontext_DecRef(ALCcontext *context)
{
    RefCount ref;
    ref = DecrementRef(&context->ref);
    TRACEREF("%p decreasing refcount to %u\n", context, ref);
    if(ref == 0) FreeContext(context);
}

static void ReleaseThreadCtx(void *ptr)
{
    WARN("%p current for thread being destroyed\n", ptr);
    ALCcontext_DecRef(ptr);
}

/* VerifyContext
 *
 * Checks that the given context is valid, and increments its reference count.
 */
static ALCcontext *VerifyContext(ALCcontext *context)
{
    ALCdevice *dev;

    LockLists();
    dev = DeviceList;
    while(dev)
    {
        ALCcontext *tmp_ctx = dev->ContextList;
        while(tmp_ctx)
        {
            if(tmp_ctx == context)
            {
                ALCcontext_IncRef(tmp_ctx);
                UnlockLists();
                return tmp_ctx;
            }
            tmp_ctx = tmp_ctx->next;
        }
        dev = dev->next;
    }
    UnlockLists();

    return NULL;
}


/* GetContextRef
 *
 * Returns the currently active context for this thread, and adds a reference
 * without locking it.
 */
ALCcontext *GetContextRef(void)
{
    ALCcontext *context;

    context = pthread_getspecific(LocalContext);
    if(context)
        ALCcontext_IncRef(context);
    else
    {
        LockLists();
        context = GlobalContext;
        if(context)
            ALCcontext_IncRef(context);
        UnlockLists();
    }

    return context;
}


/************************************************
 * Standard ALC functions
 ************************************************/

/* alcGetError
 *
 * Return last ALC generated error code for the given device
*/
ALC_API ALCenum ALC_APIENTRY alcGetError(ALCdevice *device)
{
    ALCenum errorCode;

    if(VerifyDevice(device))
    {
        errorCode = ExchangeInt(&device->LastError, ALC_NO_ERROR);
        ALCdevice_DecRef(device);
    }
    else
        errorCode = ExchangeInt(&LastNullDeviceError, ALC_NO_ERROR);

    return errorCode;
}


/* alcSuspendContext
 *
 * Not functional
 */
ALC_API ALCvoid ALC_APIENTRY alcSuspendContext(ALCcontext *Context)
{
    (void)Context;
}

/* alcProcessContext
 *
 * Not functional
 */
ALC_API ALCvoid ALC_APIENTRY alcProcessContext(ALCcontext *Context)
{
    (void)Context;
}


/* alcGetString
 *
 * Returns information about the device, and error strings
 */
ALC_API const ALCchar* ALC_APIENTRY alcGetString(ALCdevice *Device, ALCenum param)
{
    const ALCchar *value = NULL;

    switch(param)
    {
    case ALC_NO_ERROR:
        value = alcNoError;
        break;

    case ALC_INVALID_ENUM:
        value = alcErrInvalidEnum;
        break;

    case ALC_INVALID_VALUE:
        value = alcErrInvalidValue;
        break;

    case ALC_INVALID_DEVICE:
        value = alcErrInvalidDevice;
        break;

    case ALC_INVALID_CONTEXT:
        value = alcErrInvalidContext;
        break;

    case ALC_OUT_OF_MEMORY:
        value = alcErrOutOfMemory;
        break;

    case ALC_DEVICE_SPECIFIER:
        value = alcDefaultName;
        break;

    case ALC_ALL_DEVICES_SPECIFIER:
        if(VerifyDevice(Device))
        {
            value = Device->DeviceName;
            ALCdevice_DecRef(Device);
        }
        else
        {
            ProbeAllDeviceList();
            value = alcAllDeviceList;
        }
        break;

    case ALC_CAPTURE_DEVICE_SPECIFIER:
        if(VerifyDevice(Device))
        {
            value = Device->DeviceName;
            ALCdevice_DecRef(Device);
        }
        else
        {
            ProbeCaptureDeviceList();
            value = alcCaptureDeviceList;
        }
        break;

    /* Default devices are always first in the list */
    case ALC_DEFAULT_DEVICE_SPECIFIER:
        value = alcDefaultName;
        break;

    case ALC_DEFAULT_ALL_DEVICES_SPECIFIER:
        if(!alcAllDeviceList)
            ProbeAllDeviceList();

        Device = VerifyDevice(Device);

        free(alcDefaultAllDeviceSpecifier);
        alcDefaultAllDeviceSpecifier = strdup(alcAllDeviceList ?
                                              alcAllDeviceList : "");
        if(!alcDefaultAllDeviceSpecifier)
            alcSetError(Device, ALC_OUT_OF_MEMORY);

        value = alcDefaultAllDeviceSpecifier;
        if(Device) ALCdevice_DecRef(Device);
        break;

    case ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER:
        if(!alcCaptureDeviceList)
            ProbeCaptureDeviceList();

        Device = VerifyDevice(Device);

        free(alcCaptureDefaultDeviceSpecifier);
        alcCaptureDefaultDeviceSpecifier = strdup(alcCaptureDeviceList ?
                                                  alcCaptureDeviceList : "");
        if(!alcCaptureDefaultDeviceSpecifier)
            alcSetError(Device, ALC_OUT_OF_MEMORY);

        value = alcCaptureDefaultDeviceSpecifier;
        if(Device) ALCdevice_DecRef(Device);
        break;

    case ALC_EXTENSIONS:
        if(!VerifyDevice(Device))
            value = alcNoDeviceExtList;
        else
        {
            value = alcExtensionList;
            ALCdevice_DecRef(Device);
        }
        break;

    default:
        Device = VerifyDevice(Device);
        alcSetError(Device, ALC_INVALID_ENUM);
        if(Device) ALCdevice_DecRef(Device);
        break;
    }

    return value;
}


/* alcGetIntegerv
 *
 * Returns information about the device and the version of OpenAL
 */
ALC_API ALCvoid ALC_APIENTRY alcGetIntegerv(ALCdevice *device,ALCenum param,ALsizei size,ALCint *data)
{
    device = VerifyDevice(device);

    if(size == 0 || data == NULL)
    {
        alcSetError(device, ALC_INVALID_VALUE);
        if(device) ALCdevice_DecRef(device);
        return;
    }

    if(!device)
    {
        switch(param)
        {
            case ALC_MAJOR_VERSION:
                *data = alcMajorVersion;
                break;
            case ALC_MINOR_VERSION:
                *data = alcMinorVersion;
                break;

            case ALC_ATTRIBUTES_SIZE:
            case ALC_ALL_ATTRIBUTES:
            case ALC_FREQUENCY:
            case ALC_REFRESH:
            case ALC_SYNC:
            case ALC_MONO_SOURCES:
            case ALC_STEREO_SOURCES:
            case ALC_CAPTURE_SAMPLES:
            case ALC_FORMAT_CHANNELS_SOFT:
            case ALC_FORMAT_TYPE_SOFT:
                alcSetError(NULL, ALC_INVALID_DEVICE);
                break;

            default:
                alcSetError(NULL, ALC_INVALID_ENUM);
                break;
        }
    }
    else if(device->Type == Capture)
    {
        switch(param)
        {
            case ALC_CAPTURE_SAMPLES:
                LockLists();
                /* Re-validate the device since it may have been closed */
                ALCdevice_DecRef(device);
                if((device=VerifyDevice(device)) != NULL)
                    *data = ALCdevice_AvailableSamples(device);
                else
                    alcSetError(NULL, ALC_INVALID_DEVICE);
                UnlockLists();
                break;

            case ALC_CONNECTED:
                *data = device->Connected;
                break;

            default:
                alcSetError(device, ALC_INVALID_ENUM);
                break;
        }
    }
    else /* render device */
    {
        switch(param)
        {
            case ALC_MAJOR_VERSION:
                *data = alcMajorVersion;
                break;

            case ALC_MINOR_VERSION:
                *data = alcMinorVersion;
                break;

            case ALC_EFX_MAJOR_VERSION:
                *data = alcEFXMajorVersion;
                break;

            case ALC_EFX_MINOR_VERSION:
                *data = alcEFXMinorVersion;
                break;

            case ALC_ATTRIBUTES_SIZE:
                *data = 13;
                break;

            case ALC_ALL_ATTRIBUTES:
                if(size < 13)
                    alcSetError(device, ALC_INVALID_VALUE);
                else
                {
                    int i = 0;

                    data[i++] = ALC_FREQUENCY;
                    data[i++] = device->Frequency;

                    if(device->Type != Loopback)
                    {
                        data[i++] = ALC_REFRESH;
                        data[i++] = device->Frequency / device->UpdateSize;

                        data[i++] = ALC_SYNC;
                        data[i++] = ALC_FALSE;
                    }
                    else
                    {
                        data[i++] = ALC_FORMAT_CHANNELS_SOFT;
                        data[i++] = device->FmtChans;

                        data[i++] = ALC_FORMAT_TYPE_SOFT;
                        data[i++] = device->FmtType;
                    }

                    data[i++] = ALC_MONO_SOURCES;
                    data[i++] = device->NumMonoSources;

                    data[i++] = ALC_STEREO_SOURCES;
                    data[i++] = device->NumStereoSources;

                    data[i++] = ALC_MAX_AUXILIARY_SENDS;
                    data[i++] = device->NumAuxSends;

                    data[i++] = 0;
                }
                break;

            case ALC_FREQUENCY:
                *data = device->Frequency;
                break;

            case ALC_REFRESH:
                if(device->Type == Loopback)
                    alcSetError(device, ALC_INVALID_DEVICE);
                else
                    *data = device->Frequency / device->UpdateSize;
                break;

            case ALC_SYNC:
                if(device->Type == Loopback)
                    alcSetError(device, ALC_INVALID_DEVICE);
                else
                    *data = ALC_FALSE;
                break;

            case ALC_FORMAT_CHANNELS_SOFT:
                if(device->Type != Loopback)
                    alcSetError(device, ALC_INVALID_DEVICE);
                else
                    *data = device->FmtChans;
                break;

            case ALC_FORMAT_TYPE_SOFT:
                if(device->Type != Loopback)
                    alcSetError(device, ALC_INVALID_DEVICE);
                else
                    *data = device->FmtType;
                break;

            case ALC_MONO_SOURCES:
                *data = device->NumMonoSources;
                break;

            case ALC_STEREO_SOURCES:
                *data = device->NumStereoSources;
                break;

            case ALC_MAX_AUXILIARY_SENDS:
                *data = device->NumAuxSends;
                break;

            case ALC_CONNECTED:
                *data = device->Connected;
                break;

            default:
                alcSetError(device, ALC_INVALID_ENUM);
                break;
        }
    }
    if(device)
        ALCdevice_DecRef(device);
}


/* alcIsExtensionPresent
 *
 * Determines if there is support for a particular extension
 */
ALC_API ALCboolean ALC_APIENTRY alcIsExtensionPresent(ALCdevice *device, const ALCchar *extName)
{
    ALCboolean bResult = ALC_FALSE;

    device = VerifyDevice(device);

    if(!extName)
        alcSetError(device, ALC_INVALID_VALUE);
    else
    {
        size_t len = strlen(extName);
        const char *ptr = (device ? alcExtensionList : alcNoDeviceExtList);
        while(ptr && *ptr)
        {
            if(strncasecmp(ptr, extName, len) == 0 &&
               (ptr[len] == '\0' || isspace(ptr[len])))
            {
                bResult = ALC_TRUE;
                break;
            }
            if((ptr=strchr(ptr, ' ')) != NULL)
            {
                do {
                    ++ptr;
                } while(isspace(*ptr));
            }
        }
    }
    if(device)
        ALCdevice_DecRef(device);
    return bResult;
}


/* alcGetProcAddress
 *
 * Retrieves the function address for a particular extension function
 */
ALC_API ALCvoid* ALC_APIENTRY alcGetProcAddress(ALCdevice *device, const ALCchar *funcName)
{
    ALCvoid *ptr = NULL;

    device = VerifyDevice(device);

    if(!funcName)
        alcSetError(device, ALC_INVALID_VALUE);
    else
    {
        ALsizei i = 0;
        while(alcFunctions[i].funcName && strcmp(alcFunctions[i].funcName,funcName) != 0)
            i++;
        ptr = alcFunctions[i].address;
    }
    if(device)
        ALCdevice_DecRef(device);
    return ptr;
}


/* alcGetEnumValue
 *
 * Get the value for a particular ALC enumeration name
 */
ALC_API ALCenum ALC_APIENTRY alcGetEnumValue(ALCdevice *device, const ALCchar *enumName)
{
    ALCenum val = 0;

    if(!enumName)
    {
        device = VerifyDevice(device);
        alcSetError(device, ALC_INVALID_VALUE);
        if(device) ALCdevice_DecRef(device);
    }
    else
    {
        ALsizei i = 0;
        while(enumeration[i].enumName && strcmp(enumeration[i].enumName, enumName) != 0)
            i++;
        val = enumeration[i].value;
    }

    return val;
}


/* alcCreateContext
 *
 * Create and attach a context to the given device.
 */
ALC_API ALCcontext* ALC_APIENTRY alcCreateContext(ALCdevice *device, const ALCint *attrList)
{
    ALCcontext *ALContext;
    ALCenum err;

    LockLists();
    if(!(device=VerifyDevice(device)) || device->Type == Capture || !device->Connected)
    {
        UnlockLists();
        alcSetError(device, ALC_INVALID_DEVICE);
        if(device) ALCdevice_DecRef(device);
        return NULL;
    }

    device->LastError = ALC_NO_ERROR;

    if((err=UpdateDeviceParams(device, attrList)) != ALC_NO_ERROR)
    {
        UnlockLists();
        alcSetError(device, err);
        if(err == ALC_INVALID_DEVICE)
            aluHandleDisconnect(device);
        ALCdevice_DecRef(device);
        return NULL;
    }

    ALContext = calloc(1, sizeof(ALCcontext));
    if(ALContext)
    {
        ALContext->ref = 1;

        ALContext->MaxActiveSources = 256;
        ALContext->ActiveSources = malloc(sizeof(ALContext->ActiveSources[0]) *
                                          ALContext->MaxActiveSources);
    }
    if(!ALContext || !ALContext->ActiveSources)
    {
        if(!device->ContextList)
        {
            ALCdevice_StopPlayback(device);
            device->Flags &= ~DEVICE_RUNNING;
        }
        UnlockLists();

        free(ALContext);
        ALContext = NULL;

        alcSetError(device, ALC_OUT_OF_MEMORY);
        ALCdevice_DecRef(device);
        return NULL;
    }

    ALContext->Device = device;
    ALCdevice_IncRef(device);
    InitContext(ALContext);

    do {
        ALContext->next = device->ContextList;
    } while(!CompExchangePtr((XchgPtr*)&device->ContextList, ALContext->next, ALContext));
    UnlockLists();

    ALCdevice_DecRef(device);

    TRACE("Created context %p\n", ALContext);
    return ALContext;
}

/* alcDestroyContext
 *
 * Remove a context from its device
 */
ALC_API ALCvoid ALC_APIENTRY alcDestroyContext(ALCcontext *context)
{
    ALCdevice *Device;

    LockLists();
    /* alcGetContextsDevice sets an error for invalid contexts */
    Device = alcGetContextsDevice(context);
    if(Device)
    {
        ReleaseContext(context, Device);
        if(!Device->ContextList)
        {
            ALCdevice_StopPlayback(Device);
            Device->Flags &= ~DEVICE_RUNNING;
        }
    }
    UnlockLists();
}


/* alcGetCurrentContext
 *
 * Returns the currently active context on the calling thread
 */
ALC_API ALCcontext* ALC_APIENTRY alcGetCurrentContext(void)
{
    ALCcontext *Context;

    Context = pthread_getspecific(LocalContext);
    if(!Context) Context = GlobalContext;

    return Context;
}

/* alcGetThreadContext
 *
 * Returns the currently active thread-local context
 */
ALC_API ALCcontext* ALC_APIENTRY alcGetThreadContext(void)
{
    ALCcontext *Context;
    Context = pthread_getspecific(LocalContext);
    return Context;
}


/* alcMakeContextCurrent
 *
 * Makes the given context the active process-wide context, and removes the
 * thread-local context for the calling thread.
 */
ALC_API ALCboolean ALC_APIENTRY alcMakeContextCurrent(ALCcontext *context)
{
    /* context must be valid or NULL */
    if(context && !(context=VerifyContext(context)))
    {
        alcSetError(NULL, ALC_INVALID_CONTEXT);
        return ALC_FALSE;
    }
    /* context's reference count is already incremented */
    context = ExchangePtr((XchgPtr*)&GlobalContext, context);
    if(context) ALCcontext_DecRef(context);

    if((context=pthread_getspecific(LocalContext)) != NULL)
    {
        pthread_setspecific(LocalContext, NULL);
        ALCcontext_DecRef(context);
    }

    return ALC_TRUE;
}

/* alcSetThreadContext
 *
 * Makes the given context the active context for the current thread
 */
ALC_API ALCboolean ALC_APIENTRY alcSetThreadContext(ALCcontext *context)
{
    ALCcontext *old;

    /* context must be valid or NULL */
    if(context && !(context=VerifyContext(context)))
    {
        alcSetError(NULL, ALC_INVALID_CONTEXT);
        return ALC_FALSE;
    }
    /* context's reference count is already incremented */
    old = pthread_getspecific(LocalContext);
    pthread_setspecific(LocalContext, context);
    if(old) ALCcontext_DecRef(old);

    return ALC_TRUE;
}


/* alcGetContextsDevice
 *
 * Returns the device that a particular context is attached to
 */
ALC_API ALCdevice* ALC_APIENTRY alcGetContextsDevice(ALCcontext *Context)
{
    ALCdevice *Device;

    if(!(Context=VerifyContext(Context)))
    {
        alcSetError(NULL, ALC_INVALID_CONTEXT);
        return NULL;
    }
    Device = Context->Device;
    ALCcontext_DecRef(Context);

    return Device;
}


/* alcOpenDevice
 *
 * Opens the named device.
 */
ALC_API ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar *deviceName)
{
    const ALCchar *fmt;
    ALCdevice *device;
    ALCenum err;

    DO_INITCONFIG();

    if(!PlaybackBackend.name)
    {
        alcSetError(NULL, ALC_INVALID_VALUE);
        return NULL;
    }

    if(deviceName && (!deviceName[0] || strcasecmp(deviceName, alcDefaultName) == 0 || strcasecmp(deviceName, "openal-soft") == 0))
        deviceName = NULL;

    device = calloc(1, sizeof(ALCdevice)+sizeof(ALeffectslot));
    if(!device)
    {
        alcSetError(NULL, ALC_OUT_OF_MEMORY);
        return NULL;
    }

    //Validate device
    device->Funcs = &PlaybackBackend.Funcs;
    device->ref = 1;
    device->Connected = ALC_TRUE;
    device->Type = Playback;
    InitializeCriticalSection(&device->Mutex);
    device->LastError = ALC_NO_ERROR;

    device->Flags = 0;
    device->Bs2b = NULL;
    device->Bs2bLevel = 0;
    device->DeviceName = NULL;

    device->ContextList = NULL;

    device->MaxNoOfSources = 256;
    device->AuxiliaryEffectSlotMax = 4;
    device->NumAuxSends = MAX_SENDS;

    InitUIntMap(&device->BufferMap, ~0);
    InitUIntMap(&device->EffectMap, ~0);
    InitUIntMap(&device->FilterMap, ~0);

    //Set output format
    device->FmtChans = DevFmtChannelsDefault;
    device->FmtType = DevFmtTypeDefault;
    device->Frequency = DEFAULT_OUTPUT_RATE;
    device->NumUpdates = 4;
    device->UpdateSize = 1024;

    if(ConfigValueStr(NULL, "channels", &fmt))
    {
        static const struct {
            const char name[16];
            enum DevFmtChannels chans;
        } chanlist[] = {
            { "mono",       DevFmtMono   },
            { "stereo",     DevFmtStereo },
            { "quad",       DevFmtQuad   },
            { "surround51", DevFmtX51    },
            { "surround61", DevFmtX61    },
            { "surround71", DevFmtX71    },
        };
        size_t i;

        for(i = 0;i < COUNTOF(chanlist);i++)
        {
            if(strcasecmp(chanlist[i].name, fmt) == 0)
            {
                device->FmtChans = chanlist[i].chans;
                device->Flags |= DEVICE_CHANNELS_REQUEST;
                break;
            }
        }
        if(i == COUNTOF(chanlist))
            ERR("Unsupported channels: %s\n", fmt);
    }
    if(ConfigValueStr(NULL, "sample-type", &fmt))
    {
        static const struct {
            const char name[16];
            enum DevFmtType type;
        } typelist[] = {
            { "int8",    DevFmtByte   },
            { "uint8",   DevFmtUByte  },
            { "int16",   DevFmtShort  },
            { "uint16",  DevFmtUShort },
            { "int32",   DevFmtInt    },
            { "uint32",  DevFmtUInt   },
            { "float32", DevFmtFloat  },
        };
        size_t i;

        for(i = 0;i < COUNTOF(typelist);i++)
        {
            if(strcasecmp(typelist[i].name, fmt) == 0)
            {
                device->FmtType = typelist[i].type;
                device->Flags |= DEVICE_SAMPLE_TYPE_REQUEST;
                break;
            }
        }
        if(i == COUNTOF(typelist))
            ERR("Unsupported sample-type: %s\n", fmt);
    }
#define DEVICE_FORMAT_REQUEST (DEVICE_CHANNELS_REQUEST|DEVICE_SAMPLE_TYPE_REQUEST)
    if((device->Flags&DEVICE_FORMAT_REQUEST) != DEVICE_FORMAT_REQUEST &&
       ConfigValueStr(NULL, "format", &fmt))
    {
        static const struct {
            const char name[32];
            enum DevFmtChannels channels;
            enum DevFmtType type;
        } formats[] = {
            { "AL_FORMAT_MONO32",   DevFmtMono,   DevFmtFloat },
            { "AL_FORMAT_STEREO32", DevFmtStereo, DevFmtFloat },
            { "AL_FORMAT_QUAD32",   DevFmtQuad,   DevFmtFloat },
            { "AL_FORMAT_51CHN32",  DevFmtX51,    DevFmtFloat },
            { "AL_FORMAT_61CHN32",  DevFmtX61,    DevFmtFloat },
            { "AL_FORMAT_71CHN32",  DevFmtX71,    DevFmtFloat },

            { "AL_FORMAT_MONO16",   DevFmtMono,   DevFmtShort },
            { "AL_FORMAT_STEREO16", DevFmtStereo, DevFmtShort },
            { "AL_FORMAT_QUAD16",   DevFmtQuad,   DevFmtShort },
            { "AL_FORMAT_51CHN16",  DevFmtX51,    DevFmtShort },
            { "AL_FORMAT_61CHN16",  DevFmtX61,    DevFmtShort },
            { "AL_FORMAT_71CHN16",  DevFmtX71,    DevFmtShort },

            { "AL_FORMAT_MONO8",   DevFmtMono,   DevFmtByte },
            { "AL_FORMAT_STEREO8", DevFmtStereo, DevFmtByte },
            { "AL_FORMAT_QUAD8",   DevFmtQuad,   DevFmtByte },
            { "AL_FORMAT_51CHN8",  DevFmtX51,    DevFmtByte },
            { "AL_FORMAT_61CHN8",  DevFmtX61,    DevFmtByte },
            { "AL_FORMAT_71CHN8",  DevFmtX71,    DevFmtByte }
        };
        size_t i;

        ERR("Option 'format' is deprecated, please use 'channels' and 'sample-type'\n");
        for(i = 0;i < COUNTOF(formats);i++)
        {
            if(strcasecmp(fmt, formats[i].name) == 0)
            {
                if(!(device->Flags&DEVICE_CHANNELS_REQUEST))
                    device->FmtChans = formats[i].channels;
                if(!(device->Flags&DEVICE_SAMPLE_TYPE_REQUEST))
                    device->FmtType = formats[i].type;
                device->Flags |= DEVICE_FORMAT_REQUEST;
                break;
            }
        }
        if(i == COUNTOF(formats))
            ERR("Unsupported format: %s\n", fmt);
    }
#undef DEVICE_FORMAT_REQUEST

    if(ConfigValueUInt(NULL, "frequency", &device->Frequency))
    {
        device->Flags |= DEVICE_FREQUENCY_REQUEST;
        if(device->Frequency < MIN_OUTPUT_RATE)
            ERR("%uhz request clamped to %uhz minimum\n", device->Frequency, MIN_OUTPUT_RATE);
        device->Frequency = maxu(device->Frequency, MIN_OUTPUT_RATE);
    }

    ConfigValueUInt(NULL, "periods", &device->NumUpdates);
    device->NumUpdates = clampu(device->NumUpdates, 2, 16);

    ConfigValueUInt(NULL, "period_size", &device->UpdateSize);
    device->UpdateSize = clampu(device->UpdateSize, 64, 8192);

    ConfigValueUInt(NULL, "sources", &device->MaxNoOfSources);
    if(device->MaxNoOfSources == 0) device->MaxNoOfSources = 256;

    ConfigValueUInt(NULL, "slots", &device->AuxiliaryEffectSlotMax);
    if(device->AuxiliaryEffectSlotMax == 0) device->AuxiliaryEffectSlotMax = 4;

    ConfigValueUInt(NULL, "sends", &device->NumAuxSends);
    if(device->NumAuxSends > MAX_SENDS) device->NumAuxSends = MAX_SENDS;

    ConfigValueInt(NULL, "cf_level", &device->Bs2bLevel);

    device->NumStereoSources = 1;
    device->NumMonoSources = device->MaxNoOfSources - device->NumStereoSources;

    // Find a playback device to open
    LockLists();
    if((err=ALCdevice_OpenPlayback(device, deviceName)) != ALC_NO_ERROR)
    {
        UnlockLists();
        DeleteCriticalSection(&device->Mutex);
        free(device);
        alcSetError(NULL, err);
        return NULL;
    }
    UnlockLists();

    if(DefaultEffect.type != AL_EFFECT_NULL)
    {
        device->DefaultSlot = (ALeffectslot*)(device+1);
        if(InitEffectSlot(device->DefaultSlot) != AL_NO_ERROR)
        {
            device->DefaultSlot = NULL;
            ERR("Failed to initialize the default effect slot\n");
        }
        else if(InitializeEffect(device, device->DefaultSlot, &DefaultEffect) != AL_NO_ERROR)
        {
            ALeffectState_Destroy(device->DefaultSlot->EffectState);
            device->DefaultSlot = NULL;
            ERR("Failed to initialize the default effect\n");
        }
    }

    do {
        device->next = DeviceList;
    } while(!CompExchangePtr((XchgPtr*)&DeviceList, device->next, device));

    TRACE("Created device %p, \"%s\"\n", device, device->DeviceName);
    return device;
}

/* alcCloseDevice
 *
 * Closes the given device.
 */
ALC_API ALCboolean ALC_APIENTRY alcCloseDevice(ALCdevice *Device)
{
    ALCdevice *volatile*list;
    ALCcontext *ctx;

    LockLists();
    list = &DeviceList;
    while(*list && *list != Device)
        list = &(*list)->next;

    if(!*list || (*list)->Type == Capture)
    {
        alcSetError(*list, ALC_INVALID_DEVICE);
        UnlockLists();
        return ALC_FALSE;
    }

    *list = (*list)->next;
    UnlockLists();

    while((ctx=Device->ContextList) != NULL)
    {
        WARN("Releasing context %p\n", ctx);
        ReleaseContext(ctx, Device);
    }
    if((Device->Flags&DEVICE_RUNNING))
        ALCdevice_StopPlayback(Device);
    Device->Flags &= ~DEVICE_RUNNING;

    ALCdevice_ClosePlayback(Device);

    ALCdevice_DecRef(Device);

    return ALC_TRUE;
}


/************************************************
 * ALC capture functions
 ************************************************/
ALC_API ALCdevice* ALC_APIENTRY alcCaptureOpenDevice(const ALCchar *deviceName, ALCuint frequency, ALCenum format, ALCsizei samples)
{
    ALCdevice *device = NULL;
    ALCenum err;

    DO_INITCONFIG();

    if(!CaptureBackend.name)
    {
        alcSetError(NULL, ALC_INVALID_VALUE);
        return NULL;
    }

    if(samples <= 0)
    {
        alcSetError(NULL, ALC_INVALID_VALUE);
        return NULL;
    }

    if(deviceName && (!deviceName[0] || strcasecmp(deviceName, alcDefaultName) == 0 || strcasecmp(deviceName, "openal-soft") == 0))
        deviceName = NULL;

    device = calloc(1, sizeof(ALCdevice));
    if(!device)
    {
        alcSetError(NULL, ALC_OUT_OF_MEMORY);
        return NULL;
    }

    //Validate device
    device->Funcs = &CaptureBackend.Funcs;
    device->ref = 1;
    device->Connected = ALC_TRUE;
    device->Type = Capture;
    InitializeCriticalSection(&device->Mutex);

    InitUIntMap(&device->BufferMap, ~0);
    InitUIntMap(&device->EffectMap, ~0);
    InitUIntMap(&device->FilterMap, ~0);

    device->DeviceName = NULL;

    device->Flags |= DEVICE_FREQUENCY_REQUEST;
    device->Frequency = frequency;

    device->Flags |= DEVICE_CHANNELS_REQUEST | DEVICE_SAMPLE_TYPE_REQUEST;
    if(DecomposeDevFormat(format, &device->FmtChans, &device->FmtType) == AL_FALSE)
    {
        DeleteCriticalSection(&device->Mutex);
        free(device);
        alcSetError(NULL, ALC_INVALID_ENUM);
        return NULL;
    }

    device->UpdateSize = samples;
    device->NumUpdates = 1;

    LockLists();
    if((err=ALCdevice_OpenCapture(device, deviceName)) != ALC_NO_ERROR)
    {
        UnlockLists();
        DeleteCriticalSection(&device->Mutex);
        free(device);
        alcSetError(NULL, err);
        return NULL;
    }
    UnlockLists();

    do {
        device->next = DeviceList;
    } while(!CompExchangePtr((XchgPtr*)&DeviceList, device->next, device));

    TRACE("Created device %p\n", device);
    return device;
}

ALC_API ALCboolean ALC_APIENTRY alcCaptureCloseDevice(ALCdevice *Device)
{
    ALCdevice *volatile*list;

    LockLists();
    list = &DeviceList;
    while(*list && *list != Device)
        list = &(*list)->next;

    if(!*list || (*list)->Type != Capture)
    {
        alcSetError(*list, ALC_INVALID_DEVICE);
        UnlockLists();
        return ALC_FALSE;
    }

    *list = (*list)->next;
    UnlockLists();

    ALCdevice_CloseCapture(Device);

    ALCdevice_DecRef(Device);

    return ALC_TRUE;
}

ALC_API void ALC_APIENTRY alcCaptureStart(ALCdevice *device)
{
    LockLists();
    if(!(device=VerifyDevice(device)) || device->Type != Capture)
    {
        UnlockLists();
        alcSetError(device, ALC_INVALID_DEVICE);
        if(device) ALCdevice_DecRef(device);
        return;
    }
    if(device->Connected)
    {
        if(!(device->Flags&DEVICE_RUNNING))
            ALCdevice_StartCapture(device);
        device->Flags |= DEVICE_RUNNING;
    }
    UnlockLists();

    ALCdevice_DecRef(device);
}

ALC_API void ALC_APIENTRY alcCaptureStop(ALCdevice *device)
{
    LockLists();
    if(!(device=VerifyDevice(device)) || device->Type != Capture)
    {
        UnlockLists();
        alcSetError(device, ALC_INVALID_DEVICE);
        if(device) ALCdevice_DecRef(device);
        return;
    }
    if((device->Flags&DEVICE_RUNNING))
        ALCdevice_StopCapture(device);
    device->Flags &= ~DEVICE_RUNNING;
    UnlockLists();

    ALCdevice_DecRef(device);
}

ALC_API void ALC_APIENTRY alcCaptureSamples(ALCdevice *device, ALCvoid *buffer, ALCsizei samples)
{
    ALCenum err = ALC_INVALID_DEVICE;
    LockLists();
    if((device=VerifyDevice(device)) != NULL && device->Type == Capture)
    {
        err = ALC_INVALID_VALUE;
        if(samples >= 0 && ALCdevice_AvailableSamples(device) >= (ALCuint)samples)
            err = ALCdevice_CaptureSamples(device, buffer, samples);
    }
    UnlockLists();
    if(err != ALC_NO_ERROR)
        alcSetError(device, err);
    if(device) ALCdevice_DecRef(device);
}


/************************************************
 * ALC loopback functions
 ************************************************/

/* alcLoopbackOpenDeviceSOFT
 *
 * Open a loopback device, for manual rendering.
 */
ALC_API ALCdevice* ALC_APIENTRY alcLoopbackOpenDeviceSOFT(const ALCchar *deviceName)
{
    ALCdevice *device;

    DO_INITCONFIG();

    /* Make sure the device name, if specified, is us. */
    if(deviceName && strcmp(deviceName, alcDefaultName) != 0)
    {
        alcSetError(NULL, ALC_INVALID_VALUE);
        return NULL;
    }

    device = calloc(1, sizeof(ALCdevice));
    if(!device)
    {
        alcSetError(NULL, ALC_OUT_OF_MEMORY);
        return NULL;
    }

    //Validate device
    device->Funcs = &BackendLoopback.Funcs;
    device->ref = 1;
    device->Connected = ALC_TRUE;
    device->Type = Loopback;
    InitializeCriticalSection(&device->Mutex);
    device->LastError = ALC_NO_ERROR;

    device->Flags = 0;
    device->Bs2b = NULL;
    device->Bs2bLevel = 0;
    device->DeviceName = NULL;

    device->ContextList = NULL;

    device->MaxNoOfSources = 256;
    device->AuxiliaryEffectSlotMax = 4;
    device->NumAuxSends = MAX_SENDS;

    InitUIntMap(&device->BufferMap, ~0);
    InitUIntMap(&device->EffectMap, ~0);
    InitUIntMap(&device->FilterMap, ~0);

    //Set output format
    device->NumUpdates = 0;
    device->UpdateSize = 0;

    device->Frequency = DEFAULT_OUTPUT_RATE;
    device->FmtChans = DevFmtChannelsDefault;
    device->FmtType = DevFmtTypeDefault;

    ConfigValueUInt(NULL, "sources", &device->MaxNoOfSources);
    if(device->MaxNoOfSources == 0) device->MaxNoOfSources = 256;

    ConfigValueUInt(NULL, "slots", &device->AuxiliaryEffectSlotMax);
    if(device->AuxiliaryEffectSlotMax == 0) device->AuxiliaryEffectSlotMax = 4;

    ConfigValueUInt(NULL, "sends", &device->NumAuxSends);
    if(device->NumAuxSends > MAX_SENDS) device->NumAuxSends = MAX_SENDS;

    device->NumStereoSources = 1;
    device->NumMonoSources = device->MaxNoOfSources - device->NumStereoSources;

    // Open the "backend"
    ALCdevice_OpenPlayback(device, "Loopback");
    do {
        device->next = DeviceList;
    } while(!CompExchangePtr((XchgPtr*)&DeviceList, device->next, device));

    TRACE("Created device %p\n", device);
    return device;
}

/* alcIsRenderFormatSupportedSOFT
 *
 * Determines if the loopback device supports the given format for rendering.
 */
ALC_API ALCboolean ALC_APIENTRY alcIsRenderFormatSupportedSOFT(ALCdevice *device, ALCsizei freq, ALCenum channels, ALCenum type)
{
    ALCboolean ret = ALC_FALSE;

    if(!(device=VerifyDevice(device)) || device->Type != Loopback)
        alcSetError(device, ALC_INVALID_DEVICE);
    else if(freq <= 0)
        alcSetError(device, ALC_INVALID_VALUE);
    else
    {
        if(IsValidALCType(type) && BytesFromDevFmt(type) > 0 &&
           IsValidALCChannels(channels) && ChannelsFromDevFmt(channels) > 0 &&
           freq >= MIN_OUTPUT_RATE)
            ret = ALC_TRUE;
    }
    if(device) ALCdevice_DecRef(device);

    return ret;
}

/* alcRenderSamplesSOFT
 *
 * Renders some samples into a buffer, using the format last set by the
 * attributes given to alcCreateContext.
 */
ALC_API void ALC_APIENTRY alcRenderSamplesSOFT(ALCdevice *device, ALCvoid *buffer, ALCsizei samples)
{
    if(!(device=VerifyDevice(device)) || device->Type != Loopback)
        alcSetError(device, ALC_INVALID_DEVICE);
    else if(samples < 0 || (samples > 0 && buffer == NULL))
        alcSetError(device, ALC_INVALID_VALUE);
    else
        aluMixData(device, buffer, samples);
    if(device) ALCdevice_DecRef(device);
}
