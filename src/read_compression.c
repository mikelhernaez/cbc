//
//  reads_compression.c
//  XC_s2fastqIO
//
//  Created by Mikel Hernaez on 11/5/14.
//  Copyright (c) 2014 Mikel Hernaez. All rights reserved.
//


#include "read_compression.h"



/************************
 * Compress the read
 **********************/
uint32_t compress_read(Arithmetic_stream as, read_models models, read_line samLine, uint8_t chr_change){
    int tempF, PosDiff, chrPos, k;
    uint32_t mask;
    uint16_t maskedReadVal;
    // For now lets skip the unmapping ones
    if (samLine->invFlag & 4) {
//        assert(strcmp("*", samLine->cigar) == 0);
        return 1;
    }
    // compress read length (assume int)
    for (k=0;k<4;k++) {
        mask = 0xFF<<(k*8);
        maskedReadVal = (uint8_t)(models->read_length & mask)>>(k*8);
        compress_uint8t(as, models->rlength[k], maskedReadVal);
    }
    
    // Compress sam line
    PosDiff = compress_pos(as, models->pos, models->pos_alpha, samLine->pos, chr_change);
    tempF = compress_flag(as, models->flag, samLine->invFlag);
    //tempF = compress_flag(as, models->flag, 0);
    chrPos = compress_edits(as, models, samLine->edits, samLine->read, samLine->pos, PosDiff, tempF);
    
    assert(samLine->pos  == chrPos);

    return 1;
}


/***********************
 * Compress the Flag
 **********************/
uint32_t compress_flag(Arithmetic_stream a, stream_model *F, uint16_t flag){
    
    
    // In this case we need to compress the whole flag, althugh the binary information of whether the
    // read is in reverse or not is the most important. Thus, we return the binary info.
    //we use F[0] as there is no context for the flag.
    
    uint16_t x = 0;
    
    x = flag << 11;
    x >>= 15;
    
    // Send the value to the Arithmetic Stream
    send_value_to_as(a, F[0], flag);
    
    // Update model
    update_model(F[0], flag);
    
    return x;
    
}

/***********************************
 * Compress the Alphabet of Position
 ***********************************/
uint32_t compress_pos_alpha(Arithmetic_stream as, stream_model *PA, uint32_t x){
    
    uint32_t Byte = 0;
    
    // we encode byte per byte i.e. x = [B0 B1 B2 B3]
    
    // Send B0 to the Arithmetic Stream using the alphabet model
    Byte = x >> 24;
    send_value_to_as(as, PA[0], Byte);
    // Update model
    update_model(PA[0], Byte);
    
    // Send B1 to the Arithmetic Stream using the alphabet model
    Byte = (x & 0x00ff0000) >> 16;
    send_value_to_as(as, PA[1], Byte);
    // Update model
    update_model(PA[1], Byte);
    
    // Send B2 to the Arithmetic Stream using the alphabet model
    Byte = (x & 0x0000ff00) >> 8;
    send_value_to_as(as, PA[2], Byte);
    // Update model
    update_model(PA[2], Byte);
    
    // Send B3 to the Arithmetic Stream using the alphabet model
    Byte = (x & 0x000000ff);
    send_value_to_as(as, PA[3], Byte);
    // Update model
    update_model(PA[3], Byte);
    
    return 1;
    
    
}

/***********************
 * Compress the Position
 **********************/
uint32_t compress_pos(Arithmetic_stream as, stream_model *P, stream_model *PA, uint32_t pos, uint8_t chr_change){
    
    static uint32_t prevPos = 0;
    enum {SMALL_STEP = 0, BIG_STEP = 1};
    int32_t x = 0;
    
    // TODO diferent update models for updating -1 and already seen symbols
    // i.e., SMALL_STEP and BIG_STEP
    
    // Check if we are changing chromosomes.
    if (chr_change)
        prevPos = 0;
    
    
    // Compress the position diference (+ 1 to reserve 0 for new symbols)
    x = pos - prevPos + 1;
    
    if (P[0]->alphaExist[x]){
        // Send x to the Arithmetic Stream
        send_value_to_as(as, P[0], P[0]->alphaMap[x]);
        // Update model
        update_model(P[0], P[0]->alphaMap[x]);
    }
    else{
        
        // Send 0 to the Arithmetic Stream
        send_value_to_as(as, P[0], 0);
        
        // Update model
        update_model(P[0], 0);
        
        // Send the new letter to the Arithmetic Stream using the alphabet model
        compress_pos_alpha(as, PA, x);
        
        // Update the statistics of the alphabet for x
        P[0]->alphaExist[x] = 1;
        P[0]->alphaMap[x] = P[0]->alphabetCard; // We reserve the bin 0 for the new symbol flag
        P[0]->alphabet[P[0]->alphabetCard] = x;
        
        // Update model
        update_model(P[0], P[0]->alphabetCard++);
    }
    
    prevPos = pos;
    
    return x;
}

/****************************
 * Compress the match
 *****************************/
uint32_t compress_match(Arithmetic_stream a, stream_model *M, uint8_t match, uint32_t P){
    
    uint32_t ctx = 0;
    static uint8_t  prevM = 0;
    
    
    // Compute Context
    P = (P != 1)? 0:1;
    //prevP = (prevP > READ_LENGTH)? READ_LENGTH:prevP;
    //prevP = (prevP > READ_LENGTH/4)? READ_LENGTH:prevP;
    
    ctx = (P << 1) | prevM;
    
    //ctx = 0;
    
    // Send the value to the Arithmetic Stream
    send_value_to_as(a, M[ctx], match);
    
    // Update model
    update_model(M[ctx], match);
    
    prevM = match;
    
    return 1;
}

/*************************
 * Compress the snps
 *************************/
uint32_t compress_snps(Arithmetic_stream a, stream_model *S, uint8_t numSnps){
    
    
    // No context is used for the numSnps for the moment.
    
    // Send the value to the Arithmetic Stream
    send_value_to_as(a, S[0], numSnps);
    
    // Update model
    update_model(S[0], numSnps);
    
    return 1;
    
}


/********************************
 * Compress the indels
 *******************************/
uint32_t compress_indels(Arithmetic_stream a, stream_model *I, uint8_t numIndels){
    
    
    // Nos context is used for the numIndels for the moment.
    
    // Send the value to the Arithmetic Stream
    send_value_to_as(a, I[0], numIndels);
    
    // Update model
    update_model(I[0], numIndels);
    
    return 1;
    
}

/*******************************
 * Compress the variations
 ********************************/
uint32_t compress_var(Arithmetic_stream a, stream_model *v, uint32_t pos, uint32_t prevPos, uint32_t flag){
    
    uint32_t ctx = 0;
    
    //flag = 0;
    ctx = prevPos << 1 | flag;
    
    // Send the value to the Arithmetic Stream
    send_value_to_as(a, v[ctx], pos);
    
    // Update model
    update_model(v[ctx], pos);
    
    return 1;
    
}

/*****************************************
 * Compress the chars
 ******************************************/
uint32_t compress_chars(Arithmetic_stream a, stream_model *c, enum BASEPAIR ref, enum BASEPAIR target){
    
    // Send the value to the Arithmetic Stream
    send_value_to_as(a, c[ref], target);
    
    // Update model
    update_model(c[ref], target);
    
    return 1;
    
}

/*****************************************
 * Compress the edits
 ******************************************/
uint32_t compress_edits(Arithmetic_stream as, read_models rs, char *edits, char *read, uint32_t P, uint32_t deltaP, uint8_t flag){
    
    unsigned int numIns = 0, numDels = 0, numSnps = 0;
    unsigned int prevPosI = 0, prevPosD = 0, prevPosSNP = 0;
    int i = 0;

    uint32_t Dels[MAX_READ_LENGTH];
    ins Insers[MAX_READ_LENGTH];
    snp SNPs[MAX_READ_LENGTH];

    struct sequence seq;
    init_sequence(&seq, Dels, Insers, SNPs);

    uint32_t dist = edit_sequence(read, &(reference[P-1]), rs->read_length, rs->read_length, &seq);

    //uint32_t k, tempValue, tempSum = 0;
    
    
    // pos in the reference
    cumsumP = cumsumP + deltaP - 1;// DeltaP is 1-based
    

    
    
    uint32_t prev_pos = 0;
    
    
    //ALERTA AQUI y en su analogo descomp.: si pasamos por aqui no hacemos nada con el cigar... (arreglado ya?)
    if(strcmp(edits, rs->_readLength) == 0){
        // The read matches perfectly.
        compress_match(as, rs->match, 1, deltaP);
        
        return cumsumP;
    }
    
    compress_match(as, rs->match, 0, deltaP);
    
    /*
    uint32_t target_ctr = 0;
    uint32_t ref_ctr = 0;
    for (uint32_t i = 0; i < ops_len; i++) {
      switch (operations[i].edit_op) {
        case MATCH:
          target_ctr += operations[i].value;
          ref_ctr += operations[i].value;
          break;
        case REPLACE:
          SNPs[numSnps].pos = target_ctr - prevPosSNP;
          SNPs[numSnps].refChar = char2basepair(reference[target_ctr + P - 1]);
          SNPs[numSnps].targetChar = char2basepair((char) operations[i].value);
          assert(SNPs[numSnps].refChar != SNPs[numSnps].targetChar);
          prevPosSNP = target_ctr;
          target_ctr++;
          ref_ctr++;
          numSnps++;
          break;
        case INSERT:
          Insers[numIns].pos = target_ctr - prevPosI;
          Insers[numIns].targetChar = char2basepair((char) operations[i].value);
          prevPosI = target_ctr;
          target_ctr++;
          numIns++;
          break;
        case DELETE:
          for (uint32_t k = 0; k < operations[i].value; k++) {
            Dels[numDels] = ref_ctr - prevPosD;
            prevPosD = ref_ctr;
            numDels++;
            ref_ctr++;
          }
          break;
      }
    }
    
    // Compress the edits
    if ((numDels | numIns) == 0) {
        compress_snps(as, rs->snps, numSnps);
    }
    else{
        compress_snps(as, rs->snps, 0);
        compress_indels(as, rs->indels, numSnps);
        compress_indels(as, rs->indels, numDels);
        compress_indels(as, rs->indels, numIns);
    }
    
    // Store the positions and Chars in the corresponding vector
    prev_pos = 0;
    
    for (i = 0; i < numDels; i++){
        compress_var(as, rs->var, Dels[i], prev_pos, flag);
        prev_pos += Dels[i];
    }
    prev_pos = 0;
    for (i = 0; i < numSnps; i++){
        compress_var(as, rs->var, SNPs[i].pos, prev_pos, flag); 
        compress_chars(as, rs->chars, SNPs[i].refChar, SNPs[i].targetChar);
        prev_pos += SNPs[i].pos;
    }

    prev_pos = 0;
    for (i = 0; i < numIns; i++){
        compress_var(as, rs->var, Insers[i].pos, prev_pos, flag);
        prev_pos += Insers[i].pos;
        
        compress_chars(as, rs->chars, O, Insers[i].targetChar);
    }
    
    
*/
    return cumsumP;
    
}

