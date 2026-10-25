static void soft_park(CPU *c, uint32_t car){
    body_set(c,car,0);
    if(rd32(car+0x28))gcall1(c,FN_CAR_SETPHYS,car,0);
    MRF32(car + 0x60u) = 0; MRF32(car + 0x64u) = 0; MRF32(car + 0x68u) = PARK_Z;
    MRF32(car + 0x50u) = 0; MRF32(car + 0x54u) = 0; MRF32(car + 0x58u) = PARK_Z;
    for (uint32_t o = 0x70u; o < 0x7cu; o += 4) MRF32(car + o) = 0;
    for (uint32_t o = 0x110u; o < 0x11cu; o += 4) MRF32(car + o) = 0;
    MRF32(car + 0x42cu) = 0; MRF32(car + 0x430u) = 0;
    gcall0(c, FN_CAR_SECTION, car);
}