// Modified from https://github.com/jhawthorn/fzy

#include <ctype.h>
#include <math.h>

#include "common.h"
#include "fzy.h"

#define copy(data, a, b, v)                                                                        \
    do {                                                                                           \
        for (size_t i = (a); i < (b); i++) {                                                       \
            (data)[i] = (v);                                                                       \
        }                                                                                          \
    } while (0)

#define SCORE_MAX INFINITY
#define SCORE_MIN -INFINITY

#define SCORE_GAP_INNER    -0.01
#define SCORE_GAP_LEADING  -0.005
#define SCORE_GAP_TRAILING -0.005

#define SCORE_MATCH_DOT         0.6
#define SCORE_MATCH_WORD        0.8
#define SCORE_MATCH_SLASH       0.9
#define SCORE_MATCH_CAPITAL     0.7
#define SCORE_MATCH_CONSECUTIVE 1.0

static double bonus_states[3][256] = {
    {0},
    {
        ['/'] = SCORE_MATCH_SLASH,
        ['-'] = SCORE_MATCH_WORD,
        ['_'] = SCORE_MATCH_WORD,
        [' '] = SCORE_MATCH_WORD,
        ['.'] = SCORE_MATCH_DOT,
    },
    {
        ['/'] = SCORE_MATCH_SLASH,
        ['-'] = SCORE_MATCH_WORD,
        ['_'] = SCORE_MATCH_WORD,
        [' '] = SCORE_MATCH_WORD,
        ['.'] = SCORE_MATCH_DOT,
    },
};

static size_t bonus_index[256];

static int match_compare(const void *a, const void *b) {
    double sa = ((Match *) a)->score;
    double sb = ((Match *) b)->score;
    return (sa < sb) - (sa > sb);
}

static void match_calculate(Match *m, Fzy *f, Str pattern) {
    if (pattern.size == 0 || pattern.size > m->str.size) {
        m->score = SCORE_MIN;
        return;
    }

    if (pattern.size == m->str.size) {
        for (size_t i = 0; i < pattern.size; i++) {
            m->positions[i] = i;
        }

        m->score = SCORE_MAX;
        return;
    }

    f->B.count = 0;
    da_append_many(&f->B, NULL, m->str.size);

    f->D.count = 0;
    da_append_many(&f->D, NULL, pattern.size * m->str.size);

    f->M.count = 0;
    da_append_many(&f->M, NULL, pattern.size * m->str.size);

    char d = '/';
    for (size_t i = 0; i < m->str.size; i++) {
        char c = m->str.data[i];
        f->B.data[i] = bonus_states[bonus_index[c]][d];
        d = c;
    }

    double *dp = NULL;
    double *mp = NULL;
    double *dc = NULL;
    double *mc = NULL;

    for (size_t i = 0; i < pattern.size; i++) {
        dc = &f->D.data[i * m->str.size + 0];
        mc = &f->M.data[i * m->str.size + 0];

        double sp = SCORE_MIN;
        double sg = i == pattern.size - 1 ? SCORE_GAP_TRAILING : SCORE_GAP_INNER;

        for (size_t j = 0; j < m->str.size; j++) {
            if (tolower(pattern.data[i]) == tolower(m->str.data[j])) {
                double score = SCORE_MIN;
                if (i == 0) {
                    score = (j * SCORE_GAP_LEADING) + f->B.data[j];
                } else if (j) {
                    score = max(mp[j - 1] + f->B.data[j], dp[j - 1] + SCORE_MATCH_CONSECUTIVE);
                }

                dc[j] = score;
                sp = max(score, sp + sg);
            } else {
                dc[j] = SCORE_MIN;
                sp = sp + sg;
            }

            mc[j] = sp;
        }

        dp = dc;
        mp = mc;
    }

    int match_required = 0;
    for (long i = pattern.size - 1, j = m->str.size - 1; i >= 0; i--) {
        while (j >= 0) {
            if (f->D.data[i * m->str.size + j] != SCORE_MIN &&
                (match_required ||
                 f->D.data[i * m->str.size + j] == f->M.data[i * m->str.size + j])) {
                match_required =
                    i && j &&
                    f->M.data[i * m->str.size + j] ==
                        f->D.data[(i - 1) * m->str.size + j - 1] + SCORE_MATCH_CONSECUTIVE;
                m->positions[i] = j--;
                break;
            }

            j--;
        }
    }

    m->score = f->M.data[(pattern.size - 1) * m->str.size + m->str.size - 1];
}

static int fzy_has(Str pattern, Str item) {
    if (pattern.size > item.size) {
        return 0;
    }

    for (size_t i = 0, j = 0; i < pattern.size; i++) {
        char lower = tolower(pattern.data[i]);
        char upper = toupper(pattern.data[i]);

        int found = 0;
        while (j < item.size) {
            char ch = item.data[j++];
            if (ch == lower || ch == upper) {
                found = 1;
                break;
            }
        }

        if (!found) {
            return 0;
        }
    }

    return 1;
}

void fzy_init(void) {
    copy(bonus_states[2], 'a', 'z', SCORE_MATCH_CAPITAL);
    copy(bonus_index, 'A', 'Z', 2);
    copy(bonus_index, 'a', 'z', 1);
    copy(bonus_index, '0', '9', 1);
}

void fzy_free(Fzy *f) {
    da_free(&f->B);
    da_free(&f->D);
    da_free(&f->M);
    da_free(&f->matches);
    da_free(&f->positions);
}

void fzy_filter(Fzy *f, Str pattern, Str *items, size_t count) {
    f->matches.count = 0;

    f->positions.count = 0;
    da_append_many(&f->positions, NULL, pattern.size * count);

    for (size_t i = 0; i < count; i++) {
        if (fzy_has(pattern, items[i])) {
            Match match = {0};
            match.str = items[i];
            match.positions = &f->positions.data[f->matches.count * pattern.size];
            match_calculate(&match, f, pattern);

            da_append(&f->matches, match);
        }
    }

    if (pattern.size) {
        qsort(f->matches.data, f->matches.count, sizeof(*f->matches.data), match_compare);
    }
}
