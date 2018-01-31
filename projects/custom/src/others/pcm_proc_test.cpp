//#CMake Settings[environment variable]: export CMAKE_PREFIX_PATH=/usr/local/Cellar/qt/5.10.0_1/lib/cmake
//find_package(Qt5Core REQUIRED)
//find_package(Qt5Widgets REQUIRED)
//find_package(Qt5Multimedia REQUIRED)
//include_directories(${Qt5Core_INCLUDE_DIRS})
//include_directories(${Qt5Widgets_INCLUDE_DIRS})
//include_directories(${Qt5Multimedia_INCLUDE_DIRS})
//set(QtLinkLibs ${Qt5Core_LIBRARIES} ${Qt5Widgets_LIBRARIES} ${Qt5Multimedia_LIBRARIES})

#include <QApplication>
#include <QAudioFormat>
#include <QAudioOutput>
#include <unistd.h>
#include <cmath>

#define BASH_PATH "/Users/baidu/ClionProjects/untitled"

void play_pcm_le_signed(FILE *file,
                        int sample_rate, int sample_size, int channel_count) {
    fseek(file , 0, SEEK_END);
    long file_len = ftell(file);
    fseek(file, 0, SEEK_SET);

    QAudioFormat audioFormat;
    audioFormat.setSampleRate(sample_rate);
    audioFormat.setChannelCount(channel_count);
    audioFormat.setSampleSize(sample_size);
    audioFormat.setCodec("audio/pcm");
    audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    audioFormat.setSampleType(QAudioFormat::SignedInt);

    QAudioOutput *audio = new QAudioOutput(audioFormat, nullptr);
    QIODevice *device = audio->start();

    long buffer_len = static_cast<size_t>(sample_rate * channel_count * sample_size * 0.1);
    char *buffer = static_cast<char *>(malloc(static_cast<size_t>(buffer_len)));
    while (file_len - buffer_len >= 0) {
        fread(buffer, static_cast<size_t>(buffer_len), 1, file);
        device->write(buffer, static_cast<qint64>(buffer_len));
        file_len -= buffer_len;
        usleep(100000);
    }
}

int test_play(int argc, char *argv[]) {
    QApplication a(argc, argv);
    FILE *file = fopen(BASH_PATH"/a_16k_short_1_le.pcm", "r");
    play_pcm_le_signed(file, 16000, 2, 1);
    return a.exec();
}

void test_1to2() {
    FILE *input = fopen(BASH_PATH"/a_16k_short_1_le.pcm", "r");
    fseek(input , 0, SEEK_END);
    long in_file_len = ftell(input);
    fseek(input, 0, SEEK_SET);

    long in_buf_len = static_cast<size_t>(16000 * 1 * 2 * 0.1);
    char *in_buffer = static_cast<char *>(malloc(static_cast<size_t>(in_buf_len)));

    FILE *output = fopen(BASH_PATH"/a_16k_short_2_le.pcm", "w");

    long out_buf_len = in_buf_len * 2;
    char *out_buffer = static_cast<char *>(malloc(static_cast<size_t>(out_buf_len)));

    while (in_file_len - in_buf_len >= 0) {
        fread(in_buffer, static_cast<size_t>(in_buf_len), 1, input);
        in_file_len -= in_buf_len;

        for (int i = 0; i < in_buf_len; i += 2) {
            out_buffer[i * 2] = in_buffer[i];
            out_buffer[i * 2 + 1] = in_buffer[i + 1];
            out_buffer[i * 2 + 2] = in_buffer[i];
            out_buffer[i * 2 + 3] = in_buffer[i + 1];
        }

        fwrite(out_buffer, static_cast<size_t>(out_buf_len), 1, output);
    }

    fclose(input);
    fclose(output);
}

void test_2to1() {
    FILE *input = fopen(BASH_PATH"/a_44k_short_2_le.pcm", "r");
    fseek(input , 0, SEEK_END);
    long in_file_len = ftell(input);
    fseek(input, 0, SEEK_SET);

    long in_buf_len = static_cast<size_t>(44100 * 2 * 2 * 0.1);
    char *in_buffer = static_cast<char *>(malloc(static_cast<size_t>(in_buf_len)));

    FILE *output = fopen(BASH_PATH"/a_44k_short_1_le.pcm", "w");

    long out_buf_len = in_buf_len / 2;
    char *out_buffer = static_cast<char *>(malloc(static_cast<size_t>(out_buf_len)));

    while (in_file_len - in_buf_len >= 0) {
        fread(in_buffer, static_cast<size_t>(in_buf_len), 1, input);
        in_file_len -= in_buf_len;

        for (int i = 0; i < in_buf_len; i += 4) {
            short left = *(short *) &in_buffer[i];
            short right = *(short *) &in_buffer[i + 2];
            short mono = static_cast<short>((left + right) / 2);
            out_buffer[i / 2] = static_cast<char>(mono & 0xFF);
            out_buffer[i / 2 + 1] = static_cast<char>((mono >> 8) & 0xFF);
        }

        fwrite(out_buffer, static_cast<size_t>(out_buf_len), 1, output);
    }

    fclose(input);
    fclose(output);
}

void test_16to48() {
    FILE *input = fopen(BASH_PATH"/a_16k_short_1_le.pcm", "r");
    fseek(input , 0, SEEK_END);
    long in_file_len = ftell(input);
    fseek(input, 0, SEEK_SET);

    long in_buf_len = static_cast<size_t>(16000 * 1 * 2 * 0.1);
    char *in_buffer = static_cast<char *>(malloc(static_cast<size_t>(in_buf_len)));

    FILE *output = fopen(BASH_PATH"/a_48k_short_1_le1.pcm", "w");

    long out_buf_len = in_buf_len * 3;
    char *out_buffer = static_cast<char *>(malloc(static_cast<size_t>(out_buf_len)));
    short *trans_buffer = static_cast<short *>(malloc(static_cast<size_t>(out_buf_len / 2)));

    while (in_file_len - in_buf_len >= 0) {
        fread(in_buffer, static_cast<size_t>(in_buf_len), 1, input);
        in_file_len -= in_buf_len;

        for (int i = 0; i < in_buf_len; i += 2) {
            out_buffer[i * 3 + 0] = in_buffer[i];
            out_buffer[i * 3 + 1] = in_buffer[i + 1];
            out_buffer[i * 3 + 2] = 0;
            out_buffer[i * 3 + 3] = 0;
            out_buffer[i * 3 + 4] = 0;
            out_buffer[i * 3 + 5] = 0;
        }

        for (int i = 0; i < out_buf_len; i += 2) {
            double x = 0;
            if (i == 0) {
                x = 0.15 * *(short *) &out_buffer[i];
            } else if (i == 2) {
                x = 0.15 * (*(short *) &out_buffer[i]) + 0.2 * (*(short *) &out_buffer[i - 2]);
            } else if (i == 4) {
                x = 0.15 * (*(short *) &out_buffer[i]) + 0.2 * (*(short *) &out_buffer[i - 2])
                    + 0.3 * (*(short *) &out_buffer[i - 4]);
            } else if (i == 6) {
                x = 0.15 * (*(short *) &out_buffer[i]) + 0.2 * (*(short *) &out_buffer[i - 2])
                    + 0.3 * (*(short *) &out_buffer[i - 4]) + 0.2 * (*(short *) &out_buffer[i - 6]);
            } else {
                x = 0.15 * (*(short *) &out_buffer[i]) + 0.2 * (*(short *) &out_buffer[i - 2])
                    + 0.3 * (*(short *) &out_buffer[i - 4]) + 0.2 * (*(short *) &out_buffer[i - 6])
                    + 0.15 * (*(short *) &out_buffer[i - 8]);
            }
            trans_buffer[i / 2] = static_cast<short>(x);
        }

        fwrite(trans_buffer, 2, static_cast<size_t>(out_buf_len / 2), output);
    }

    fclose(input);
    fclose(output);
}

void test_44to11() {
    FILE *input = fopen(BASH_PATH"/a_44k_short_1_le.pcm", "r");
    fseek(input , 0, SEEK_END);
    long in_file_len = ftell(input);
    fseek(input, 0, SEEK_SET);

    long in_buf_len = static_cast<size_t>(44100 * 1 * 2 * 0.1);
    char *in_buffer = static_cast<char *>(malloc(static_cast<size_t>(in_buf_len)));
    short *trans_buffer = static_cast<short *>(malloc(static_cast<size_t>(in_buf_len)));

    FILE *output = fopen(BASH_PATH"/a_11k_short_1_le.pcm", "w");

    while (in_file_len - in_buf_len >= 0) {
        fread(in_buffer, static_cast<size_t>(in_buf_len), 1, input);
        in_file_len -= in_buf_len;

        for (int i = 0; i < in_buf_len; i += 2) {
            double x = 0;
            if (i == 0) {
                x = 0.15 * *(short *) &in_buffer[i];
            } else if (i == 2) {
                x = 0.15 * (*(short *) &in_buffer[i]) + 0.2 * (*(short *) &in_buffer[i - 2]);
            } else if (i == 4) {
                x = 0.15 * (*(short *) &in_buffer[i]) + 0.2 * (*(short *) &in_buffer[i - 2])
                    + 0.3 * (*(short *) &in_buffer[i - 4]);
            } else if (i == 6) {
                x = 0.15 * (*(short *) &in_buffer[i]) + 0.2 * (*(short *) &in_buffer[i - 2])
                    + 0.3 * (*(short *) &in_buffer[i - 4]) + 0.2 * (*(short *) &in_buffer[i - 6]);
            } else {
                x = 0.15 * (*(short *) &in_buffer[i]) + 0.2 * (*(short *) &in_buffer[i - 2])
                    + 0.3 * (*(short *) &in_buffer[i - 4]) + 0.2 * (*(short *) &in_buffer[i - 6])
                    + 0.15 * (*(short *) &in_buffer[i - 8]);
            }
            trans_buffer[i / 2] = static_cast<short>(x);
        }

        for(int i = 0; i < (in_buf_len / 2); i+=4) {
            fwrite(&trans_buffer[i], 2, 1, output);
        }
    }

    fclose(input);
    fclose(output);
}

void test_float2short() {
    FILE *input = fopen(BASH_PATH"/a_16k_float_1_le.pcm", "r");
    fseek(input , 0, SEEK_END);
    long in_file_len = ftell(input);
    fseek(input, 0, SEEK_SET);

    long in_buf_len = static_cast<size_t>(44100 * 4 * 1 * 0.1);
    char *in_buffer = static_cast<char *>(malloc(static_cast<size_t>(in_buf_len)));

    FILE *output = fopen(BASH_PATH"/a_16k_short_1_le.pcm", "w");

    long out_buf_len = in_buf_len / 2;
    char *out_buffer = static_cast<char *>(malloc(static_cast<size_t>(out_buf_len)));

    while (in_file_len - in_buf_len >= 0) {
        fread(in_buffer, static_cast<size_t>(in_buf_len), 1, input);
        in_file_len -= in_buf_len;

        for (int i = 0; i < in_buf_len; i += 4) {
            float val = (*(float *) &in_buffer[i]) * 32768;
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;
            short t = static_cast<short>(val);
            out_buffer[i / 2] = static_cast<char>(t & 0xFF);
            out_buffer[i / 2 + 1] = static_cast<char>((t >> 8) & 0xFF);
        }

        fwrite(out_buffer, static_cast<size_t>(out_buf_len), 1, output);
    }

    fclose(input);
    fclose(output);
}

void test_mix(bool method1) {
    FILE *inputA = fopen(BASH_PATH"/a.pcm", "r");
    fseek(inputA , 0, SEEK_END);
    long in_fileA_len = ftell(inputA);
    fseek(inputA, 0, SEEK_SET);

    FILE *inputB = fopen(BASH_PATH"/b.pcm", "r");
    fseek(inputB , 0, SEEK_END);
    long in_fileB_len = ftell(inputB);
    fseek(inputB, 0, SEEK_SET);

    FILE *output = fopen(BASH_PATH"/c.pcm", "w");
    long out_file_len = in_fileA_len > in_fileB_len ? in_fileB_len : in_fileA_len;

    long buf_len = static_cast<long>(16000 * 2 * 1 * 0.1);
    char *bufferA = static_cast<char *>(malloc(static_cast<size_t>(buf_len)));
    char *bufferB = static_cast<char *>(malloc(static_cast<size_t>(buf_len)));
    char *bufferC = static_cast<char *>(malloc(static_cast<size_t>(buf_len)));

    while (out_file_len - buf_len >= 0) {
        fread(bufferA, static_cast<size_t>(buf_len), 1, inputA);
        fread(bufferB, static_cast<size_t>(buf_len), 1, inputB);
        out_file_len -= buf_len;

        for (int i = 0; i < buf_len; i += 2) {
            short a = *((short *) &bufferA[i]);
            short b = *((short *) &bufferB[i]);

            short val = 0;
            if (method1) {
                val = static_cast<short>((a + b) / 2);
            } else {
                if( a < 0 && b < 0) {
                    val = static_cast<short>(a + b - (a * b / -(pow(2, 16 - 1) - 1)));
                } else {
                    val = static_cast<short>(a + b - (a * b / (pow(2, 16 - 1) - 1)));
                }
            }

            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;
            bufferC[i] = static_cast<char>(val & 0xFF);
            bufferC[i + 1] = static_cast<char>((val >> 8) & 0xFF);
        }

        fwrite(bufferC, static_cast<size_t>(buf_len), 1, output);
    }

    fclose(inputA);
    fclose(inputB);
    fclose(output);
}

int main() {
    return 0;
}