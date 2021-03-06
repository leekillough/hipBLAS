/* ************************************************************************
 * Copyright 2016 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "cblas_interface.h"
#include "flops.h"
#include "hipblas.hpp"
#include "norm.h"
#include "unit.h"
#include "utility.h"

using namespace std;

/* ============================================================================================ */

template <typename T>
hipblasStatus_t testing_trtri(Arguments argus)
{

    int N = argus.N;
    int lda;
    int ldinvA;
    ldinvA = lda = argus.lda;

    int A_size = lda * N;

    hipblasStatus_t status = HIPBLAS_STATUS_SUCCESS;

    // check here to prevent undefined memory allocation error
    if(N < 0 || lda < 0)
    {
        return HIPBLAS_STATUS_INVALID_VALUE;
    }
    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    vector<T> hA(A_size);
    vector<T> hB(A_size);

    T *dA, *dinvA;

    double gpu_time_used, cpu_time_used;
    double hipblasGflops, cblas_gflops;
    double rocblas_error;

    hipblasHandle_t handle;

    char char_uplo = argus.uplo_option;
    char char_diag = argus.diag_option;

    hipblasFillMode_t uplo = char2hipblasFillMode_t(char_uplo);
    hipblasDiagType_t diag = char2hipblasDiagType_t(char_diag);

    hipblasCreate(&handle);

    // allocate memory on device
    CHECK_HIP_ERROR(hipMalloc(&dA, A_size * sizeof(T)));
    CHECK_HIP_ERROR(hipMalloc(&dinvA, A_size * sizeof(T)));

    // Initial Data on CPU
    srand(1);
    hipblas_init_symmetric<T>(hA, N, lda);

    // proprocess the matrix to avoid ill-conditioned matrix
    for(int i = 0; i < N; i++)
    {
        for(int j = 0; j < N; j++)
        {
            hA[i + j * lda] *= 0.01;

            if(j % 2)
                hA[i + j * lda] *= -1;
            if(i == j)
            {
                if(diag == HIPBLAS_DIAG_UNIT)
                    hA[i + j * lda] = 1.0;
                else
                    hA[i + j * lda] *= 100.0;
            }
        }
    }
    hB = hA;

    // copy data from CPU to device
    CHECK_HIP_ERROR(hipMemcpy(dA, hA.data(), sizeof(T) * A_size, hipMemcpyHostToDevice));

    /* =====================================================================
           ROCBLAS
    =================================================================== */
    status = hipblasTrtri<T>(handle, uplo, diag, N, dA, lda, dinvA, ldinvA);

    // copy output from device to CPU
    CHECK_HIP_ERROR(hipMemcpy(hA.data(), dinvA, sizeof(T) * A_size, hipMemcpyDeviceToHost));

    if(argus.unit_check)
    {
        /* =====================================================================
           CPU BLAS
        =================================================================== */

        int info = cblas_trtri<T>(char_uplo, char_diag, N, hB.data(), lda);

        if(info != 0)
            printf("error in cblas_trtri\n");

#ifndef NDEBUG
        print_matrix(hB, hA, N, N, lda);
#endif

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(argus.unit_check)
        {
            unit_check_general<T>(N, N, lda, hB.data(), hA.data());
        }
    }

    CHECK_HIP_ERROR(hipFree(dA));
    CHECK_HIP_ERROR(hipFree(dinvA));
    hipblasDestroy(handle);
    return HIPBLAS_STATUS_SUCCESS;
}
