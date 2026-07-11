#include <cstdio>
#include <cstring>
#include <cctype>
#include <unistd.h>
#include <string>
#include <vector>
#include <mdr/Headphones.hpp>
#include "Platform/Platform.hpp"

static int mystrcasecmp(const char* a, const char* b)
{
    for (; *a && *b; a++, b++)
    {
        int ca = toupper((unsigned char)*a);
        int cb = toupper((unsigned char)*b);
        if (ca != cb) return ca - cb;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static mdr::MDRHeadphones gDevice;

static int waitForEvent(int expected, int maxPolls = 3000)
{
    for (int i = 0; i < maxPolls; i++)
    {
        int ev = gDevice.PollEvents();
        if (ev == expected) return 0;
        if (ev < 0 && ev != MDR_HEADPHONES_INPROGRESS) return ev;
        usleep(10000); // 10ms between polls
    }
    return -1;
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
    printf("  Battery L: %d%% (charging=%d)\n", gDevice.mBatteryL.level, (int)gDevice.mBatteryL.charging);
    printf("  Battery R: %d%% (charging=%d)\n", gDevice.mBatteryR.level, (int)gDevice.mBatteryR.charging);
    printf("  Case:      %d%% (charging=%d)\n", gDevice.mBatteryCase.level, (int)gDevice.mBatteryCase.charging);
}

static void printEq()
{
    using enum mdr::v2::t1::EqPresetId;
    const char* names[] = {"Off","Rock","Pop","Jazz","Dance","EDM","R&B","Acoustic",
        "Bright","Excited","Mellow","Relaxed","Vocal","Treble","Bass","Speech",
        "Heavy","Clear","Hard","Soft","Gaming","FPS1","FPS2","FPS3","Custom",
        "User1","User2","User3","User4","User5"};
    int id = (int)gDevice.mEqPresetId.current;
    const char* pname = (id >= 0 && id < 30) ? names[id] : "?";
    printf("  EQ Preset: %s (%d)\n", pname, id);
    printf("  Clear Bass: %d\n", gDevice.mEqClearBass.current);
    printf("  Bands (%zu):", gDevice.mEqConfig.current.size());
    for (auto b : gDevice.mEqConfig.current) printf(" %+d", b);
    printf("\n");
}

static void printSoundEffect()
{
    if (!gDevice.mSupport.contains(F1::SOUND_EFFECT)) return;
    printf("  Sound Effect: %d\n", (int)gDevice.mSoundEffect.current);
}

static void printUltMode()
{
    if (!gDevice.mSupport.contains(F1::PRESET_EQ_AND_ULT_MODE)) return;
    printf("  ULT Mode: %d\n", (int)gDevice.mEqUltMode.current);
}

static mdr::v2::t1::EqPresetId parsePreset(const char* s)
{
    using P = mdr::v2::t1::EqPresetId;
    struct { const char* name; P id; } map[] = {
        {"OFF",P::OFF},{"ROCK",P::ROCK},{"POP",P::POP},{"JAZZ",P::JAZZ},{"DANCE",P::DANCE},
        {"EDM",P::EDM},{"RNB",P::R_AND_B_HIP_HOP},{"ACOUSTIC",P::ACOUSTIC},
        {"BRIGHT",P::BRIGHT},{"EXCITED",P::EXCITED},{"MELLOW",P::MELLOW},{"RELAXED",P::RELAXED},
        {"VOCAL",P::VOCAL},{"TREBLE",P::TREBLE},{"BASS",P::BASS},{"SPEECH",P::SPEECH},
        {"HEAVY",P::HEAVY},{"CLEAR",P::CLEAR},{"HARD",P::HARD},{"SOFT",P::SOFT},
        {"GAMING",P::GAMING_EQ},{"FPS1",P::FPS_1},{"FPS2",P::FPS_2},{"FPS3",P::FPS_3},
        {"CUSTOM",P::CUSTOM},{"USER1",P::USER_SETTING1},{"USER2",P::USER_SETTING2},
        {"USER3",P::USER_SETTING3},{"USER4",P::USER_SETTING4},{"USER5",P::USER_SETTING5},
    };
    for (auto& m : map)
        if (!mystrcasecmp(s, m.name)) return m.id;
    return P::OFF;
}

static bool connect(const char* mac)
{
    fprintf(stderr, "Connecting to %s...\n", mac);

    // Step 1: Init BT stack
    if (clientPlatformConnectionInit(0) != MDR_RESULT_OK)
    { fprintf(stderr, "Failed to init Bluetooth\n"); return false; }

    auto* conn = clientPlatformConnectionGet();
    if (!conn) { fprintf(stderr, "No connection object\n"); return false; }

    // Step 2: Don't pre-connect via bluetoothctl — it interferes with RFCOMM.
    // The MDR connection opens its own RFCOMM socket directly.

    // Step 3: Connect MDR protocol over RFCOMM
    fprintf(stderr, "MDR connecting...\n");
    int res = mdrConnectionConnect(conn, mac, MDR_SERVICE_UUID_XM5);
    int polls = 0;
    while (polls < 300)
    {
        if (res == MDR_RESULT_OK) break;
        if (res != MDR_RESULT_INPROGRESS && res != MDR_RESULT_ERROR_TIMEOUT)
        {
            fprintf(stderr, "  connect error: res=%d err=%s\n", res, mdrConnectionGetLastError(conn));
            break;
        }
        res = mdrConnectionPoll(conn, 100);
        polls++;
    }
    if (res != MDR_RESULT_OK)
    { fprintf(stderr, "MDR failed: %s (polls=%d)\n", mdrConnectionGetLastError(conn), polls); return false; }

    // Step 4: Init MDR session
    gDevice = mdr::MDRHeadphones(conn);
    if (gDevice.Invoke(gDevice.RequestInitV2()) != MDR_RESULT_OK)
    { fprintf(stderr, "Init invoke failed\n"); return false; }

    fprintf(stderr, "Waiting for init to complete...\n");
    int ret = waitForEvent(MDR_HEADPHONES_TASK_INIT_OK, 600);
    if (ret != 0) { fprintf(stderr, "Init failed: %d\n", ret); return false; }
    fprintf(stderr, "Init OK\n");

    fprintf(stderr, "Connected to %s\n", gDevice.mModelName.c_str());
    return true;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: mdr-cli <mac> [command...]\n");
        fprintf(stderr, "  With no command: interactive mode\n");
        fprintf(stderr, "  Commands: status, battery, eq, sound-effect, ult-mode, ...\n");
        return 1;
    }

    const char* mac = argv[1];

    // now-playing: local only, no BT needed
    if (argc >= 2 && !strcmp(argv[1], "now-playing"))
    {
        FILE* fp = popen(
            "python3 -c \"import dbus; bus=dbus.SessionBus(); "
            "p=bus.get_object('org.mpris.MediaPlayer2.elisa','/org/mpris/MediaPlayer2'); "
            "m=dbus.Interface(p,'org.freedesktop.DBus.Properties').Get("
            "'org.mpris.MediaPlayer2.Player','Metadata'); "
            "print('Title:', m.get('xesam:title','?')); "
            "print('Artist:', ', '.join(m.get('xesam:artist',['?]))); "
            "print('Album:', m.get('xesam:album','?'))\"", "r");
        if (!fp) return 1;
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) printf("%s", buf);
        pclose(fp);
        return 0;
    }

    // Connect to device (persistent connection)
    if (!connect(mac)) return 1;

    using namespace mdr::v2;
    using namespace mdr::v2::t1;

    // Single-command mode (remainder of argv is the command)
    if (argc > 2)
    {
        const char* cmd = argv[2];
        if (!strcmp(cmd, "status"))
        {
            doSync(); doSync();
            printf("Model: %s\n", gDevice.mModelName.c_str());
            printf("FW: %s\n", gDevice.mFWVersion.c_str());
            printBattery();
            printEq(); printUltMode(); printSoundEffect();
            printf("  Upscaling: %s\n", gDevice.mUpscalingEnabled.current ? "ON" : "OFF");
            printf("  Volume: %d\n", gDevice.mPlayVolume.current);
        }
        else if (!strcmp(cmd, "battery"))
        { doSync(); printBattery(); }
        else if (!strcmp(cmd, "eq"))
        {
            printEq(); printUltMode(); printSoundEffect();
        }
        else if (!strcmp(cmd, "sound-effect") && argc > 3)
        {
            int v = atoi(argv[3]);
            gDevice.mSoundEffect.desired = (SoundEffectType)v;
            doCommit();
            printf("Sound Effect set to %d\n", v);
        }
        else if (!strcmp(cmd, "ult-mode") && argc > 3)
        {
            int v = atoi(argv[3]);
            gDevice.mEqUltMode.desired = (EqUltMode)v;
            if (argc > 4) gDevice.mEqPresetId.desired = parsePreset(argv[4]);
            doCommit();
            printf("ULT Mode set\n");
        }
        else if (!strcmp(cmd, "set-eq") && argc > 4)
        {
            gDevice.mEqPresetId.desired = parsePreset(argv[3]);
            std::vector<int> bands;
            for (int i = 4; i < argc; i++) bands.push_back(atoi(argv[i]));
            gDevice.mEqConfig.desired = bands;
            doCommit();
            printf("EQ set\n");
        }
        else if (!strcmp(cmd, "volume") && argc > 3)
        {
            gDevice.mPlayVolume.desired = atoi(argv[3]);
            doCommit();
            printf("Volume set to %d\n", gDevice.mPlayVolume.desired);
        }
        else
        {
            fprintf(stderr, "Unknown command: %s\n", cmd);
        }
        return 0;
    }

    // Interactive mode
    printf("Connected to %s. Type 'help' for commands, 'quit' to exit.\n",
           gDevice.mModelName.c_str());

    char line[256];
    while (true)
    {
        printf("> "); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        char* nl = strchr(line, '\n'); if (nl) *nl = 0;
        if (!line[0] || !strcmp(line, "quit") || !strcmp(line, "exit")) break;

        if (!strcmp(line, "help"))
        {
            printf("Commands:\n");
            printf("  status         - full device status\n");
            printf("  battery        - battery levels\n");
            printf("  eq             - EQ/ULT/sound-effect status\n");
            printf("  snd <0-6>      - set sound effect (0=OFF 1=ULT 2=ULT1 3=ULT2 4=CUSTOM 5=FLAT 6=LIVE)\n");
            printf("  ult <0-2> [p]  - set ULT mode (0=OFF 1=ULT1 2=ULT2) with optional preset\n");
            printf("  eqset <p> b... - set EQ preset + bands\n");
            printf("  vol <0-30>     - set volume\n");
            printf("  quit/exit      - disconnect and exit\n");
        }
        else if (!strcmp(line, "status"))
        {
            doSync(); doSync();
            printf("Model: %s  FW: %s\n", gDevice.mModelName.c_str(), gDevice.mFWVersion.c_str());
            printBattery();
            printEq(); printUltMode(); printSoundEffect();
            printf("  Volume: %d  Upscaling: %s\n",
                   gDevice.mPlayVolume.current,
                   gDevice.mUpscalingEnabled.current ? "ON" : "OFF");
        }
        else if (!strcmp(line, "battery"))
        { doSync(); printBattery(); }
        else if (!strcmp(line, "eq"))
        { printEq(); printUltMode(); printSoundEffect(); }
        else if (!strncmp(line, "snd ", 4))
        {
            int v = atoi(line + 4);
            gDevice.mSoundEffect.desired = (SoundEffectType)v;
            doCommit();
            printf("Sound Effect: %d\n", v);
        }
        else if (!strncmp(line, "ult ", 4))
        {
            char* p = line + 4;
            int v = atoi(p);
            gDevice.mEqUltMode.desired = (EqUltMode)v;
            while (*p && *p != ' ') p++;
            if (*p) { p++; gDevice.mEqPresetId.desired = parsePreset(p); }
            doCommit();
            printf("ULT Mode set\n");
        }
        else if (!strncmp(line, "eqset ", 6))
        {
            char* p = line + 6;
            char* preset = p;
            while (*p && *p != ' ') p++;
            if (*p) { *p++ = 0; }
            gDevice.mEqPresetId.desired = parsePreset(preset);
            std::vector<int> bands;
            while (*p)
            {
                while (*p == ' ') p++;
                if (!*p) break;
                bands.push_back(atoi(p));
                while (*p && *p != ' ') p++;
            }
            gDevice.mEqConfig.desired = bands;
            doCommit();
            printf("EQ set: preset=%s bands=%zu\n", preset, bands.size());
        }
        else if (!strncmp(line, "vol ", 4))
        {
            gDevice.mPlayVolume.desired = atoi(line + 4);
            doCommit();
            printf("Volume: %d\n", gDevice.mPlayVolume.desired);
        }
        else
        {
            printf("Unknown: %s\n", line);
        }
    }

    return 0;
}
