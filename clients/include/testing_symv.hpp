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
hipblasStatus_t testing_symv(Arguments argus)
{

    int N    = argus.N;
    int lda  = argus.lda;
    int incx = argus.incx;
    int incy = argus.incy;

    int A_size = lda * N;
    int X_size = N * incx;
    int Y_size = N * incy;

    hipblasStatus_t status = HIPBLAS_STATUS_SUCCESS;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(N < 0)
    {
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }
    else if(lda < 0)
    {
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }
    else if(incx < 0)
    {
        status = HIPBLAS_STATUS_INVALID_VALUE;
        return status;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    vector<T> hA(A_size);
    vector<T> hx(X_size);
    vector<T> hy(Y_size);
    vector<T> hz(Y_size);

    T *dA, *dx, *dy;

    double gpu_time_used, cpu_time_used;
    double hipblasGflops, cblas_gflops;
    double rocblas_error;

    T alpha = (T)argus.alpha;
    T beta  = (T)argus.beta;

    hipblasHandle_t handle;

    char              char_fill = argus.uplo_option;
    hipblasFillMode_t uplo      = char2hipblasFillMode_t(char_fill);

    hipblasCreate(&handle);

    // allocate memory on device
    CHECK_HIP_ERROR(hipMalloc(&dA, A_size * sizeof(T)));
    CHECK_HIP_ERROR(hipMalloc(&dx, X_size * sizeof(T)));
    CHECK_HIP_ERROR(hipMalloc(&dy, Y_size * sizeof(T)));

    // Initial Data on CPU
    srand(1);
    hipblas_init_symmetric<T>(hA, N, lda);
    hipblas_init<T>(hx, 1, N, incx);
    hipblas_init<T>(hy, 1, N, incy);

    // copy vector is easy in STL; hz = hy: save a copy in hz which will be output of CPU BLAS
    hz = hy;

    // copy data from CPU to device
    hipMemcpy(dA, hA.data(), sizeof(T) * lda * N, hipMemcpyHostToDevice);
    hipMemcpy(dx, hx.data(), sizeof(T) * N * incx, hipMemcpyHostToDevice);
    hipMemcpy(dy, hy.data(), sizeof(T) * N * incy, hipMemcpyHostToDevice);

    /* =====================================================================
           ROCBLAS
    =================================================================== */

    for(int iter = 0; iter < 1; iter++)
    {

        status
            = hipblasSymv<T>(handle, uplo, N, (T*)&alpha, dA, lda, dx, incx, (T*)&beta, dy, incy);

        if(status != HIPBLAS_STATUS_SUCCESS)
        {
            CHECK_HIP_ERROR(hipFree(dA));
            CHECK_HIP_ERROR(hipFree(dx));
            CHECK_HIP_ERROR(hipFree(dy));
            hipblasDestroy(handle);
            return status;
        }
    }

    // copy output from device to CPU
    hipMemcpy(hy.data(), dy, sizeof(T) * N * incy, hipMemcpyDeviceToHost);

    if(argus.unit_check)
    {
        /* =====================================================================
           CPU BLAS
        =================================================================== */
        cblas_symv<T>(uplo, N, alpha, hA.data(), lda, hx.data(), incx, beta, hz.data(), incy);

        // enable unit check, notice unit check is not invasive, but norm check is,
        // unit check and norm check can not be interchanged their order
        if(argus.unit_check)
        {
            unit_check_general<T>(1, N, incy, hz.data(), hy.data());
        }

        for(int i = 0; i < 32; i++)
        {
            printf("CPU[%d]=%f, GPU[%d]=%f\n", i, hz[i], i, hy[i]);
        }
    }

    CHECK_HIP_ERROR(hipFree(dA));
    CHECK_HIP_ERROR(hipFree(dx));
    CHECK_HIP_ERROR(hipFree(dy));
    hipblasDestroy(handle);
    return HIPBLAS_STATUS_SUCCESS;
}
