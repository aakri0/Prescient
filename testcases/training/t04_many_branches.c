/* t04_many_branches.c
 *
 * Complexity pattern: very high control-flow complexity from a long
 * if/else chain (30+ arms) and a large switch statement.
 *
 * Why it is interesting to the model: cyclomatic complexity here is well
 * above 20. Passes that reason about control flow — SimplifyCFG, jump
 * threading, SCCP — do proportionally more work. This is a MEDIUM-HIGH
 * tier sample that decouples branch complexity from loop complexity.
 */

int classify(int x) {
    if (x < 0)        return -1;
    else if (x == 0)  return 0;
    else if (x < 5)   return 1;
    else if (x < 10)  return 2;
    else if (x < 15)  return 3;
    else if (x < 20)  return 4;
    else if (x < 25)  return 5;
    else if (x < 30)  return 6;
    else if (x < 35)  return 7;
    else if (x < 40)  return 8;
    else if (x < 45)  return 9;
    else if (x < 50)  return 10;
    else if (x < 55)  return 11;
    else if (x < 60)  return 12;
    else if (x < 65)  return 13;
    else if (x < 70)  return 14;
    else if (x < 75)  return 15;
    else if (x < 80)  return 16;
    else if (x < 85)  return 17;
    else if (x < 90)  return 18;
    else if (x < 95)  return 19;
    else if (x < 100) return 20;
    else if (x < 110) return 21;
    else if (x < 120) return 22;
    else if (x < 130) return 23;
    else if (x < 140) return 24;
    else if (x < 150) return 25;
    else if (x < 160) return 26;
    else if (x < 170) return 27;
    else if (x < 180) return 28;
    else if (x < 190) return 29;
    else if (x < 200) return 30;
    else              return 31;
}

int dispatch(int code) {
    switch (code) {
        case 0:  return 100;
        case 1:  return 101;
        case 2:  return 102;
        case 3:  return 103;
        case 4:  return 104;
        case 5:  return 105;
        case 6:  return 106;
        case 7:  return 107;
        case 8:  return 108;
        case 9:  return 109;
        case 10: return 110;
        case 11: return 111;
        case 12: return 112;
        default: return -1;
    }
}

int combined(int x, int code) {
    int c = classify(x);
    if (c < 0)
        return dispatch(0);
    if (c > 15)
        return dispatch(code) + c;
    return dispatch(code);
}
