#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <mdr/Headphones.hpp>
#include "Platform/Platform.hpp"

static mdr::MDRHeadphones gDevice;
static int gLastEvent = 0;

static int waitForEvent(int expected, int maxPolls = 300)
{
    for (int i = 0; i < maxPolls; i++)
    {
        gLastEvent = gDevice.PollEvents();
        if (gLastEvent == expected) return 0;
        if (gLastEvent < 0 && gLastEvent != MDR_HEADPHONES_INPROGRESS) return gLastEvent;
    }
    return -1;
}

static int waitForAny(int maxPolls = 300)
{
    for (int i = 0; i < maxPolls; i++)
    {
        gLastEvent = gDevice.PollEvents();
        if (gLastEvent != MDR_HEADPHONES_IDLE && gLastEvent != MDR_HEADPHONES_INPROGRESS)
            return gLastEvent;
    }
    return -1;
}

static int connect(const char* mac)
{
    auto* conn = clientPlatformConnectionGet();
    if (!conn) { fprintf(stderr, "No connection object\n"); return -1; }

    int res = mdrConnectionConnect(conn, mac, MDR_SERVICE_UUID_XM5);
    while (res == MDR_RESULT_INPROGRESS)
        res = mdrConnectionPoll(conn, 100);
    if (res != MDR_RESULT_OK)
    { fprintf(stderr, "Connect failed: %s\n", mdrConnectionGetLastError(conn)); return -1; }

    gDevice = mdr::MDRHeadphones(conn);
    if (gDevice.Invoke(gDevice.RequestInitV2()) != MDR_RESULT_OK)
    { fprintf(stderr, "Invoke init failed\n"); return -1; }

    int ret = waitForEvent(MDR_HEADPHONES_TASK_INIT_OK);
    if (ret != 0) { fprintf(stderr, "Init failed: %d\n", ret); return -1; }
    return 0;
}

static int doSync()
{
    if (gDevice.Invoke(gDevice.RequestSyncV2()) != MDR_RESULT_OK) return -1;
    return waitForEvent(MDR_HEADPHONES_TASK_SYNC_OK);
}

static int doCommit()
{
    if (gDevice.Invoke(gDevice.RequestCommitV2()) != MDR_RESULT_OK) return -1;
    return waitForEvent(MDR_HEADPHONES_TASK_COMMIT_OK);
}

using F1 = mdr::v2::MessageMdrV2FunctionType_Table1;

static void printBattery()
{
    printf("  Battery L: %d%% (charging=%d, threshold=%d)\n",
           gDevice.mBatteryL.level, (int)gDevice.mBatteryL.charging, gDevice.mBatteryL.threshold);
    printf("  Battery R: %d%% (charging=%d, threshold=%d)\n",
           gDevice.mBatteryR.level, (int)gDevice.mBatteryR.charging, gDevice.mBatteryR.threshold);
    printf("  Case:      %d%% (charging=%d, threshold=%d)\n",
           gDevice.mBatteryCase.level, (int)gDevice.mBatteryCase.charging, gDevice.mBatteryCase.threshold);
}

static void printCodec()
{
    using enum mdr::v2::t1::AudioCodec;
    switch (gDevice.mAudioCodec)
    {
    case SBC: printf("  Codec: SBC\n"); break;
    case AAC: printf("  Codec: AAC\n"); break;
    case LDAC: printf("  Codec: LDAC\n"); break;
    case APT_X: printf("  Codec: aptX\n"); break;
    case APT_X_HD: printf("  Codec: aptX HD\n"); break;
    case LC3: printf("  Codec: LC3\n"); break;
    default: printf("  Codec: Unknown (%d)\n", (int)gDevice.mAudioCodec); break;
    }
}

static const char* presetName(mdr::v2::t1::EqPresetId id)
{
    using enum mdr::v2::t1::EqPresetId;
    switch (id)
    {
    case OFF: return "Off";
    case ROCK: return "Rock";
    case POP: return "Pop";
    case JAZZ: return "Jazz";
    case DANCE: return "Dance";
    case EDM: return "EDM";
    case R_AND_B_HIP_HOP: return "R&B/Hip-Hop";
    case ACOUSTIC: return "Acoustic";
    case BRIGHT: return "Bright";
    case EXCITED: return "Excited";
    case MELLOW: return "Mellow";
    case RELAXED: return "Relaxed";
    case VOCAL: return "Vocal";
    case TREBLE: return "Treble";
    case BASS: return "Bass";
    case SPEECH: return "Speech";
    case HEAVY: return "Heavy";
    case CLEAR: return "Clear";
    case HARD: return "Hard";
    case SOFT: return "Soft";
    case GAMING_EQ: return "Gaming";
    case FPS_1: return "FPS 1";
    case FPS_2: return "FPS 2";
    case FPS_3: return "FPS 3";
    case CUSTOM: return "Custom";
    case USER_SETTING1: return "User 1";
    case USER_SETTING2: return "User 2";
    case USER_SETTING3: return "User 3";
    case USER_SETTING4: return "User 4";
    case USER_SETTING5: return "User 5";
    default: return "Unknown";
    }
}

static void printEq()
{
    printf("  EQ Preset: %s (%d)\n", presetName(gDevice.mEqPresetId.current), (int)gDevice.mEqPresetId.current);
    printf("  Clear Bass: %d\n", gDevice.mEqClearBass.current);
    printf("  Bands (%zu):", gDevice.mEqConfig.current.size());
    for (auto b : gDevice.mEqConfig.current) printf(" %+d", b);
    printf("\n");
}

static void printUltMode()
{
    if (!gDevice.mSupport.contains(F1::PRESET_EQ_AND_ULT_MODE)) return;
    using enum mdr::v2::t1::EqUltMode;
    printf("  ULT Mode: ");
    switch (gDevice.mEqUltMode.current)
    {
    case OFF: printf("Off\n"); break;
    case ULT_1: printf("ULT1 - enhances deeper low-pass range\n"); break;
    case ULT_2: printf("ULT2 - increased sense of power\n"); break;
    default: printf("Unknown (%d)\n", (int)gDevice.mEqUltMode.current); break;
    }
}

static void printSoundEffect()
{
    if (!gDevice.mSupport.contains(F1::SOUND_EFFECT)) return;
    using enum mdr::v2::t1::SoundEffectType;
    printf("  Sound Effect: ");
    switch (gDevice.mSoundEffect.current)
    {
    case SOUND_EFFECT_OFF: printf("Off\n"); break;
    case SOUND_EFFECT_ULT: printf("ULT POWER SOUND - enhances low-pass range\n"); break;
    case SOUND_EFFECT_ULT1: printf("ULT1 - enhances deeper low-pass range\n"); break;
    case SOUND_EFFECT_ULT2: printf("ULT2 - increased sense of power\n"); break;
    case SOUND_EFFECT_CUSTOM: printf("Custom\n"); break;
    case SOUND_EFFECT_FLAT: printf("Flat\n"); break;
    case SOUND_EFFECT_LIVE: printf("Live\n"); break;
    default: printf("Unknown (%d)\n", (int)gDevice.mSoundEffect.current); break;
    }
}

static void printNcAsm()
{
    bool hasNC = gDevice.mSupport.contains(F1::NOISE_CANCELLING_ONOFF);
    bool hasASM = gDevice.mSupport.contains(F1::AMBIENT_SOUND_MODE_ONOFF)
        || gDevice.mSupport.contains(F1::AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT);
    printf("  NC/ASM: %s Mode=%s Level=%d FocusOnVoice=%s\n",
           gDevice.mNcAsmEnabled.current ? "ON" : "OFF",
           gDevice.mNcAsmMode.current == mdr::v2::t1::NcAsmMode::NC ? "NC" : "ASM",
           gDevice.mNcAsmAmbientLevel.current,
           gDevice.mNcAsmFocusOnVoice.current ? "yes" : "no");
    if (gDevice.mSupport.contains(F1::MODE_NC_ASM_NOISE_CANCELLING_DUAL_AMBIENT_SOUND_MODE_LEVEL_ADJUSTMENT_NOISE_ADAPTATION))
        printf("  Auto ASM: %s Sensitivity=%d\n",
               gDevice.mNcAsmAutoAsmEnabled.current ? "ON" : "OFF",
               (int)gDevice.mNcAsmNoiseAdaptiveSensitivity.current);
}

static void printPlaying()
{
    printf("  Now Playing: %s / %s / %s\n",
           gDevice.mPlayTrackTitle.c_str(),
           gDevice.mPlayTrackAlbum.c_str(),
           gDevice.mPlayTrackArtist.c_str());
    printf("  Volume: %d  Playback: %s\n",
           gDevice.mPlayVolume.current,
           gDevice.mPlayPause == mdr::v2::t1::PlaybackStatus::PLAY ? "Playing" : "Paused");
}

// --- Argument parsing helpers ---

static mdr::v2::t1::EqPresetId parsePreset(const char* s)
{
    using enum mdr::v2::t1::EqPresetId;
    if (!strcmp(s, "OFF") || !strcmp(s, "Off")) return OFF;
    if (!strcmp(s, "ROCK") || !strcmp(s, "Rock")) return ROCK;
    if (!strcmp(s, "POP") || !strcmp(s, "Pop")) return POP;
    if (!strcmp(s, "JAZZ") || !strcmp(s, "Jazz")) return JAZZ;
    if (!strcmp(s, "DANCE") || !strcmp(s, "Dance")) return DANCE;
    if (!strcmp(s, "EDM")) return EDM;
    if (!strcmp(s, "R&B") || !strcmp(s, "RNB")) return R_AND_B_HIP_HOP;
    if (!strcmp(s, "ACOUSTIC") || !strcmp(s, "Acoustic")) return ACOUSTIC;
    if (!strcmp(s, "BRIGHT") || !strcmp(s, "Bright")) return BRIGHT;
    if (!strcmp(s, "EXCITED") || !strcmp(s, "Excited")) return EXCITED;
    if (!strcmp(s, "MELLOW") || !strcmp(s, "Mellow")) return MELLOW;
    if (!strcmp(s, "RELAXED") || !strcmp(s, "Relaxed")) return RELAXED;
    if (!strcmp(s, "VOCAL") || !strcmp(s, "Vocal")) return VOCAL;
    if (!strcmp(s, "TREBLE") || !strcmp(s, "Treble")) return TREBLE;
    if (!strcmp(s, "BASS") || !strcmp(s, "Bass")) return BASS;
    if (!strcmp(s, "SPEECH") || !strcmp(s, "Speech")) return SPEECH;
    if (!strcmp(s, "HEAVY") || !strcmp(s, "Heavy")) return HEAVY;
    if (!strcmp(s, "CLEAR") || !strcmp(s, "Clear")) return CLEAR;
    if (!strcmp(s, "HARD") || !strcmp(s, "Hard")) return HARD;
    if (!strcmp(s, "SOFT") || !strcmp(s, "Soft")) return SOFT;
    if (!strcmp(s, "GAMING") || !strcmp(s, "Gaming")) return GAMING_EQ;
    if (!strcmp(s, "FPS1")) return FPS_1;
    if (!strcmp(s, "FPS2")) return FPS_2;
    if (!strcmp(s, "FPS3")) return FPS_3;
    if (!strcmp(s, "CUSTOM") || !strcmp(s, "Custom")) return CUSTOM;
    return OFF;
}

static void showHelp(const char* prog)
{
    fprintf(stderr,
    "Usage: %s <mac> <command> [args...]\n"
    "\n"
    "Commands:\n"
    "  now-playing                     Show currently playing song (no BT needed)\n"
    "  status                          Show all device info and current state\n"
    "  get-battery                     Read battery levels\n"
    "  get-eq                          Show current EQ preset and bands\n"
    "\n"
    "  sound-effect <mode>             Set sound effect\n"
    "    Modes: OFF, ULT, ULT1, ULT2, CUSTOM, FLAT, LIVE\n"
    "\n"
    "  ult-mode <mode> [preset]        Set ULT mode with optional EQ preset\n"
    "    Modes: OFF, ULT1, ULT2\n"
    "    Preset: any EQ preset name\n"
    "\n"
    "  eq <preset> [band1 band2 ...]   Set EQ preset with optional band values\n"
    "    For 5-band EQ: <clearBass> <400> <1k> <2.5k> <6.3k> <16k>\n"
    "      Range: clearBass -10..10, bands -10..10\n"
    "    For 10-band EQ: <31> <63> <125> <250> <500> <1k> <2k> <4k> <8k> <16k>\n"
    "      Range: all -6..6\n"
    "    Presets: OFF, ROCK, POP, JAZZ, DANCE, EDM, ACOUSTIC,\n"
    "      BRIGHT, EXCITED, MELLOW, RELAXED, VOCAL, TREBLE, BASS, SPEECH,\n"
    "      HEAVY, CLEAR, HARD, SOFT, GAMING, FPS1, FPS2, FPS3, CUSTOM\n"
    "\n"
    "  nc-asm <mode> [level]           Set NC/ASM mode\n"
    "    Modes: NC, ASM, OFF\n"
    "    Level: 1-20 (ambient sound strength, default 20)\n"
    "\n"
    "  volume <0-30>                   Set volume\n"
    "  upscaling <ON|OFF>              Toggle DSEE upscaling\n"
    "  playback <play|pause|next|prev> Playback control\n"
    "  power-off                       Shutdown the device\n"
    "  pairing <ON|OFF>                Toggle pairing mode\n"
    "\n"
    "  list-functions                  List all supported functions\n"
    , prog);
}

int main(int argc, char** argv)
{
    // Commands that don't need MAC or BT connection
    if (argc >= 2 && strcmp(argv[1], "now-playing") == 0)
    {
        // now-playing: read from MPRIS directly (no BT needed)
        FILE* fp = popen(
            "python3 -c \"import dbus; bus=dbus.SessionBus(); "
            "p=bus.get_object('org.mpris.MediaPlayer2.elisa','/org/mpris/MediaPlayer2'); "
            "m=dbus.Interface(p,'org.freedesktop.DBus.Properties').Get("
            "'org.mpris.MediaPlayer2.Player','Metadata'); "
            "print('Title:', m.get('xesam:title','?')); "
            "print('Artist:', ', '.join(m.get('xesam:artist',['?']))); "
            "print('Album:', m.get('xesam:album','?'))\"", "r");
        if (!fp) { fprintf(stderr, "Failed to read MPRIS\n"); return 1; }
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) printf("%s", buf);
        pclose(fp);
        printf("\nTip: run 'mpris-proxy &' to forward metadata to Bluetooth\n");
        return 0;
    }

    if (argc < 3)
    {
        showHelp(argv[0]);
        return 1;
    }

    const char* mac = argv[1];
    const char* cmd = argv[2];

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "help") == 0)
    { showHelp(argv[0]); return 0; }
    {
        FILE* fp = popen(
            "python3 -c \"import dbus; bus=dbus.SessionBus(); "
            "p=bus.get_object('org.mpris.MediaPlayer2.elisa','/org/mpris/MediaPlayer2'); "
            "m=dbus.Interface(p,'org.freedesktop.DBus.Properties').Get("
            "'org.mpris.MediaPlayer2.Player','Metadata'); "
            "print('Title:', m.get('xesam:title','?')); "
            "print('Artist:', ', '.join(m.get('xesam:artist',['?']))); "
            "print('Album:', m.get('xesam:album','?'))\"", "r");
        if (!fp) { fprintf(stderr, "Failed to read MPRIS\n"); return 1; }
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) printf("%s", buf);
        pclose(fp);
        printf("\nTip: run 'mpris-proxy &' to forward metadata to Bluetooth\n");
        return 0;
    }

    // Connect
    if (clientPlatformConnectionInit(0) != MDR_RESULT_OK)
    { fprintf(stderr, "Failed to init Bluetooth\n"); return 1; }

    if (connect(mac) != 0) return 1;

    using namespace mdr::v2;
    using namespace mdr::v2::t1;

    // --- STATUS ---
    if (strcmp(cmd, "status") == 0)
    {
        doSync();
        doSync(); // second sync for battery
        printf("Device: %s\n", gDevice.mModelName.c_str());
        printf("  MAC: %s\n", gDevice.mUniqueId.c_str());
        printf("  FW: %s\n", gDevice.mFWVersion.c_str());
        printf("  Series: %d  Color: %d\n", (int)gDevice.mModelSeries, (int)gDevice.mModelColor);
        printCodec();
        printf("\n");
        printBattery();
        printf("\n");
        printNcAsm();
        printf("\n");
        printEq();
        printUltMode();
        printSoundEffect();
        printf("\n");
        printf("  Upscaling: %s (%s available)\n",
               gDevice.mUpscalingEnabled.current ? "ON" : "OFF",
               gDevice.mUpscalingAvailable ? "" : "not ");
        printf("  Audio Priority: %d\n", (int)gDevice.mAudioPriorityMode.current);
        printf("  Auto Pause: %s\n", gDevice.mAutoPauseEnabled.current ? "ON" : "OFF");
        printf("  Speak-to-Chat: %s\n", gDevice.mSpeakToChatEnabled.current ? "ON" : "OFF");
        printf("  Head Gesture: %s\n", gDevice.mHeadGestureEnabled.current ? "ON" : "OFF");
        printf("  Voice Guidance: %s (vol=%d)\n",
               gDevice.mVoiceGuidanceEnabled.current ? "ON" : "OFF",
               gDevice.mVoiceGuidanceVolume.current);
        printf("\n");
        printPlaying();
        return 0;
    }

    // --- GET-BATTERY ---
    if (strcmp(cmd, "get-battery") == 0)
    {
        doSync();
        printBattery();
        return 0;
    }

    // --- GET-EQ ---
    if (strcmp(cmd, "get-eq") == 0)
    {
        printEq();
        printUltMode();
        printSoundEffect();
        return 0;
    }

    // --- SOUND-EFFECT ---
    if (strcmp(cmd, "sound-effect") == 0)
    {
        if (argc < 4) { fprintf(stderr, "Usage: mdr-cli <mac> sound-effect <mode>\n"); return 1; }
        if (!gDevice.mSupport.contains(F1::SOUND_EFFECT))
        { fprintf(stderr, "Device does not support SOUND_EFFECT\n"); return 1; }
        const char* val = argv[3];
        using enum SoundEffectType;
        if (!strcmp(val, "OFF")) gDevice.mSoundEffect.desired = SOUND_EFFECT_OFF;
        else if (!strcmp(val, "ULT")) gDevice.mSoundEffect.desired = SOUND_EFFECT_ULT;
        else if (!strcmp(val, "ULT1")) gDevice.mSoundEffect.desired = SOUND_EFFECT_ULT1;
        else if (!strcmp(val, "ULT2")) gDevice.mSoundEffect.desired = SOUND_EFFECT_ULT2;
        else if (!strcmp(val, "CUSTOM")) gDevice.mSoundEffect.desired = SOUND_EFFECT_CUSTOM;
        else if (!strcmp(val, "FLAT")) gDevice.mSoundEffect.desired = SOUND_EFFECT_FLAT;
        else if (!strcmp(val, "LIVE")) gDevice.mSoundEffect.desired = SOUND_EFFECT_LIVE;
        else { fprintf(stderr, "Unknown mode: %s (use OFF|ULT|ULT1|ULT2|CUSTOM|FLAT|LIVE)\n", val); return 1; }
        if (doCommit() != 0) { fprintf(stderr, "Commit failed\n"); return 1; }
        printf("Sound Effect set to %s\n", val);
        return 0;
    }

    // --- ULT-MODE ---
    if (strcmp(cmd, "ult-mode") == 0)
    {
        if (argc < 4) { fprintf(stderr, "Usage: mdr-cli <mac> ult-mode <mode> [preset]\n"); return 1; }
        if (!gDevice.mSupport.contains(F1::PRESET_EQ_AND_ULT_MODE))
        { fprintf(stderr, "Device does not support PRESET_EQ_AND_ULT_MODE\n"); return 1; }
        using enum EqUltMode;
        if (!strcmp(argv[3], "OFF")) gDevice.mEqUltMode.desired = OFF;
        else if (!strcmp(argv[3], "ULT1")) gDevice.mEqUltMode.desired = ULT_1;
        else if (!strcmp(argv[3], "ULT2")) gDevice.mEqUltMode.desired = ULT_2;
        else { fprintf(stderr, "Unknown mode: %s\n", argv[3]); return 1; }
        if (argc > 4) gDevice.mEqPresetId.desired = parsePreset(argv[4]);
        if (doCommit() != 0) { fprintf(stderr, "Commit failed\n"); return 1; }
        printf("ULT Mode set to %s", argv[3]);
        if (argc > 4) printf(", EQ preset: %s", argv[4]);
        printf("\n");
        return 0;
    }

    // --- EQ ---
    if (strcmp(cmd, "eq") == 0)
    {
        if (argc < 4) { fprintf(stderr, "Usage: mdr-cli <mac> eq <preset> [bands...]\n"); return 1; }
        gDevice.mEqPresetId.desired = parsePreset(argv[3]);
        if (argc > 4)
        {
            std::vector<int> bands;
            for (int i = 4; i < argc; i++)
                bands.push_back(atoi(argv[i]));
            gDevice.mEqConfig.desired = bands;
        }
        if (doCommit() != 0) { fprintf(stderr, "Commit failed\n"); return 1; }
        printf("EQ set to %s", argv[3]);
        if (argc > 4) printf(" with %d bands", argc - 4);
        printf("\n");
        return 0;
    }

    // --- NC-ASM ---
    if (strcmp(cmd, "nc-asm") == 0)
    {
        if (argc < 4) { fprintf(stderr, "Usage: mdr-cli <mac> nc-asm <NC|ASM|OFF> [level]\n"); return 1; }
        if (!strcmp(argv[3], "OFF"))
            gDevice.mNcAsmEnabled.desired = false;
        else
        {
            gDevice.mNcAsmEnabled.desired = true;
            if (!strcmp(argv[3], "NC")) gDevice.mNcAsmMode.desired = NcAsmMode::NC;
            else if (!strcmp(argv[3], "ASM")) gDevice.mNcAsmMode.desired = NcAsmMode::ASM;
            else { fprintf(stderr, "Unknown mode: %s\n", argv[3]); return 1; }
        }
        if (argc > 4) gDevice.mNcAsmAmbientLevel.desired = atoi(argv[4]);
        if (doCommit() != 0) { fprintf(stderr, "Commit failed\n"); return 1; }
        printf("NC/ASM set\n");
        return 0;
    }

    // --- VOLUME ---
    if (strcmp(cmd, "volume") == 0)
    {
        if (argc < 4) { fprintf(stderr, "Usage: mdr-cli <mac> volume <0-30>\n"); return 1; }
        gDevice.mPlayVolume.desired = atoi(argv[3]);
        if (doCommit() != 0) { fprintf(stderr, "Commit failed\n"); return 1; }
        printf("Volume set to %d\n", gDevice.mPlayVolume.desired);
        return 0;
    }

    // --- UPSCALING ---
    if (strcmp(cmd, "upscaling") == 0)
    {
        if (argc < 4) { fprintf(stderr, "Usage: mdr-cli <mac> upscaling <ON|OFF>\n"); return 1; }
        gDevice.mUpscalingEnabled.desired = !strcmp(argv[3], "ON");
        if (doCommit() != 0) { fprintf(stderr, "Commit failed\n"); return 1; }
        printf("Upscaling %s\n", gDevice.mUpscalingEnabled.desired ? "ON" : "OFF");
        return 0;
    }

    // --- PLAYBACK ---
    if (strcmp(cmd, "playback") == 0)
    {
        if (argc < 4) { fprintf(stderr, "Usage: mdr-cli <mac> playback <play|pause|next|prev>\n"); return 1; }
        using enum PlaybackControl;
        if (!strcmp(argv[3], "play")) gDevice.mPlayControl.desired = PLAY;
        else if (!strcmp(argv[3], "pause")) gDevice.mPlayControl.desired = PAUSE;
        else if (!strcmp(argv[3], "next")) gDevice.mPlayControl.desired = TRACK_UP;
        else if (!strcmp(argv[3], "prev")) gDevice.mPlayControl.desired = TRACK_DOWN;
        else { fprintf(stderr, "Unknown: %s\n", argv[3]); return 1; }
        if (doCommit() != 0) { fprintf(stderr, "Commit failed\n"); return 1; }
        printf("Playback: %s\n", argv[3]);
        return 0;
    }

    // --- POWER-OFF ---
    if (strcmp(cmd, "power-off") == 0)
    {
        gDevice.mShutdown.desired = true;
        if (doCommit() != 0) { fprintf(stderr, "Commit failed\n"); return 1; }
        printf("Powering off...\n");
        return 0;
    }

    // --- LIST-FUNCTIONS ---
    if (strcmp(cmd, "list-functions") == 0)
    {
        printf("Supported Table 1 functions:\n");
        for (int i = 0; i < 256; i++)
        {
            auto ft = static_cast<MessageMdrV2FunctionType_Table1>(i);
            if (is_valid(ft) && gDevice.mSupport.contains(ft))
                printf("  0x%02X %s\n", i, format_as(ft));
        }
        printf("Supported Table 2 functions:\n");
        for (int i = 0; i < 256; i++)
        {
            auto ft = static_cast<MessageMdrV2FunctionType_Table2>(i);
            if (is_valid(ft) && gDevice.mSupport.contains(ft))
                printf("  0x%02X %s\n", i, format_as(ft));
        }
        return 0;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    showHelp(argv[0]);
    return 1;
}
