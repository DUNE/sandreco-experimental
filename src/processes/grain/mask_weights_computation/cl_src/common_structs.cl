CL_STRUCT(typedef struct {
  float4 top_o;
  float4 top_n;
  float4 rgt_o;
  float4 rgt_n;
  float4 bot_o;
  float4 bot_n;
  float4 lft_o;
  float4 lft_n;
  int3 idx;
} frustum_t;)

CL_STRUCT(typedef struct {
  float4 x;
  float4 y;
  float4 z;
  float4 w;
} transform_t;)

CL_STRUCT(typedef struct {
  float bottom;
  float left;
  float top;
  float right;
} rect_f;)

CL_STRUCT(typedef struct {
  float voxel_size;
  float lar_attenuation_length;
  float pde;
  int minivoxels_per_side;
} solidangle_cfg;)
