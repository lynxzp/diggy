#pragma once

// ============================================================================
// Utility functions
// ============================================================================
static u16 htons(u16 x){ return (u16)((x<<8)|(x>>8)); }

static int itoa(int val, char* buf){
    int i = 0, j, tmp;
    char rev[12];
    if(val == 0){ buf[0] = '0'; return 1; }
    while(val > 0){
        rev[i++] = '0' + (val % 10);
        val /= 10;
    }
    for(j = 0; j < i; j++) buf[j] = rev[i-j-1];
    return i;
}

static void* memcpy_manual(char* dst, const char* src, int n){
    int i;
    for(i = 0; i < n; i++) {
        dst[i] = src[i];
    }
    return dst;
}
