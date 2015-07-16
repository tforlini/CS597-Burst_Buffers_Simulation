/*
 * Copyright (C) 2013 University of Chicago.
 * See COPYRIGHT notice in top-level directory.
 *
 */

#ifndef CODESIOKERNELCONTEXT_H
#define CODESIOKERNELCONTEXT_H

typedef struct CodesIOKernelContext
{
    void *scanner_;
    char *text;
    void *lval;
    void *locval;

    int64_t sym[26];
    int64_t var[10];
    int64_t next_var[10];

    int inst_ready;

    /* Ning's additions for the bgp storage model */
    /* XXX Does this belong here? */
    int GroupRank;
    int GroupSize;

} CodesIOKernelContext;

void CodesIOKernelScannerInit(
    CodesIOKernelContext * context);
void CodesIOKernelScannerDestroy(
    CodesIOKernelContext * context);

void CodesIOKernelScannerSetSymTable(
    CodesIOKernelContext * context);

#endif

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
