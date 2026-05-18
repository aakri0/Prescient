/* t07_large_function.c
 *
 * Complexity pattern: one large function (200+ lines) mixing loops,
 * conditionals and helper-function calls, alongside two supporting
 * helpers. The body is long enough that instruction_count and
 * call_site_count are both high.
 *
 * Why it is interesting to the model: raw function size is a baseline
 * predictor — nearly every pass scales at least linearly with the
 * instruction count. Combined with branches and calls, this is a
 * HIGH-tier sample.
 */

#include <stdlib.h>

static int clamp(int value, int lo, int hi) {
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static int mix(int a, int b, int rounds) {
    int acc = a ^ b;
    for (int i = 0; i < rounds; i++) {
        acc = (acc << 1) | (acc >> 31);
        acc ^= (a + i);
        acc += (b - i);
    }
    return acc;
}

int run_pipeline(int *buffer, int length, int mode, int seed) {
    int total = 0;
    int positive = 0;
    int negative = 0;
    int zero = 0;
    int running = seed;
    int peak = 0;
    int trough = 0;

    /* Phase 1: normalise the buffer in place. */
    for (int i = 0; i < length; i++) {
        int v = buffer[i];
        if (v > 1000)
            v = 1000;
        else if (v < -1000)
            v = -1000;
        buffer[i] = clamp(v, -500, 500);
    }

    /* Phase 2: gather first-order statistics. */
    for (int i = 0; i < length; i++) {
        int v = buffer[i];
        total += v;
        if (v > 0)
            positive++;
        else if (v < 0)
            negative++;
        else
            zero++;
        if (v > peak)
            peak = v;
        if (v < trough)
            trough = v;
    }

    /* Phase 3: mode-dependent transformation. */
    if (mode == 0) {
        for (int i = 0; i < length; i++) {
            running = mix(running, buffer[i], 3);
            buffer[i] = running & 0xFFFF;
        }
    } else if (mode == 1) {
        for (int i = 0; i < length; i++) {
            int left = (i > 0) ? buffer[i - 1] : 0;
            int right = (i < length - 1) ? buffer[i + 1] : 0;
            buffer[i] = (left + buffer[i] + right) / 3;
        }
    } else if (mode == 2) {
        for (int i = 0; i < length; i++) {
            if (buffer[i] % 2 == 0)
                buffer[i] = buffer[i] / 2;
            else
                buffer[i] = buffer[i] * 3 + 1;
        }
    } else {
        for (int i = 0; i < length; i++)
            buffer[i] = clamp(buffer[i] + seed, -500, 500);
    }

    /* Phase 4: windowed accumulation with an inner reduction loop. */
    int window_sum = 0;
    for (int i = 0; i < length; i++) {
        int window = 0;
        for (int w = -2; w <= 2; w++) {
            int idx = i + w;
            if (idx >= 0 && idx < length)
                window += buffer[idx];
        }
        window_sum += window;
        if (window > peak * 5)
            window_sum += 1;
    }

    /* Phase 5: derive a score from the gathered metrics. */
    int score = 0;
    if (positive > negative) {
        score = positive - negative;
        if (zero > 0)
            score += zero;
    } else if (negative > positive) {
        score = negative - positive;
        score = -score;
    } else {
        score = zero;
    }

    if (total > 0)
        score += total / (length > 0 ? length : 1);
    else
        score -= 1;

    /* Phase 6: a small state machine over the running value. */
    int state = 0;
    for (int i = 0; i < length; i++) {
        switch (state) {
            case 0:
                if (buffer[i] > 0)
                    state = 1;
                break;
            case 1:
                if (buffer[i] < 0)
                    state = 2;
                else
                    score += 1;
                break;
            case 2:
                if (buffer[i] == 0)
                    state = 0;
                else
                    score -= 1;
                break;
            default:
                state = 0;
                break;
        }
    }

    /* Phase 7: final fold combining everything. */
    int result = score + window_sum + peak + trough;
    result = mix(result, total, 4);
    result = clamp(result, -100000, 100000);
    return result;
}
