//
// Created by seeta on 2018/7/19.
//

#ifndef TENSORSTACK_KERNELS_CPU_MATH_CPU_H
#define TENSORSTACK_KERNELS_CPU_MATH_CPU_H

#include "core/tensor.h"
#include "../common/blas.h"

namespace ts {
    namespace cpu {
        template <typename T>
        class math {
        public:
            static void check(const Tensor &tensor) {
                if (tensor.device().type() != CPU) throw DeviceMismatchException(Device(CPU), tensor.device());
            }

            static T dot(
                    int N,
                    const T *x,
                    int incx,
                    const T *y,
                    int incy
                    );

            static void gemm(
                    blas::Order Order,
                    blas::Transpose TransA,
                    blas::Transpose TransB,
                    int M, int N, int K,
                    T alpha,
                    const T *A, int lda,
                    const T *B, int ldb,
                    T beta,
                    T *C, int ldc);
        };


    }
}

extern template class ts::cpu::math<ts::type<ts::FLOAT32>::declare>;
extern template class ts::cpu::math<ts::type<ts::FLOAT64>::declare>;


#endif //TENSORSTACK_KERNELS_CPU_MATH_CPU_H