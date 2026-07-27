#pragma once
#include <cstdio>

#if defined(_WIN32) || defined(_WIN64)
    #include <io.h>
    #define DUP         _dup
    #define DUP2        _dup2
    #define FILENO      _fileno
    #define NULL_FILE   "NUL"
#else
    #include <unistd.h>
    #define DUP         dup
    #define DUP2        dup2
    #define FILENO      fileno
    #define NULL_FILE   "/dev/null"
#endif

class Silencer
{
    private:
        static inline const int stdoutFileNo{DUP(STDOUT_FILENO)};
        static inline const int stderrFileNo{DUP(STDERR_FILENO)};
        FILE* nullFile{};

        void flushStreams()
        {
            fflush(stdout);
            fflush(stderr);
        }

    public:
        static inline bool silenced{false};

        Silencer() : nullFile{fopen(NULL_FILE, "w")} {}

        void silence()
        {
            flushStreams();
            int nullFileNo{FILENO(nullFile)};
            DUP2(nullFileNo, STDOUT_FILENO);
            DUP2(nullFileNo, STDERR_FILENO);
            silenced = true;
        }

        void restore()
        {
            flushStreams();
            DUP2(stdoutFileNo, STDOUT_FILENO);
            DUP2(stderrFileNo, STDERR_FILENO);
            silenced = false;
        }

        ~Silencer()
        {
            fclose(nullFile);
        }
};

#undef DUP
#undef DUP2
#undef FILENO
#undef NULL_FILE