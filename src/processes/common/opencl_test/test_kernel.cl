OCL_KERNEL(void vector_add(__global const float* A, __global const float* B, __global float* C, ulong N) {
  size_t i = get_global_id(0);
  if (i < N) C[i] = A[i] + B[i];
})
