/*Copyright (c) 2011, Edgar Solomonik, all rights reserved.*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <math.h>
#include <assert.h>
#include <algorithm>
#include <ctf.hpp>
#include "../src/shared/util.h"

int  ccsdt_t3_to_t2(int const  n,
                    int const  m,
                    char const *dir){
  int rank, i, num_pes;
  int64_t np;
  double * pairs;
  int64_t * indices;
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_pes);

  CTF_World dw;
#if DEBUG  >= 1
  if (rank == 0)
    printf("n = %d\n", n);
#endif
 
  int shapeAS4[] = {AS,NS,AS,NS};
  int shapeAS6[] = {AS,AS,NS,AS,AS,NS};
  int shapeHS6[] = {AS,AS,NS,NS,NS,NS};
  int shapeTS6[] = {AS,NS,NS,NS,NS,NS};
  int shapeTS4[] = {AS,NS,NS,NS};
  int shapeNS4[] = {NS,NS,NS,NS};
  //int shapeNS6[] = {NS,NS,NS,NS,NS,NS};
  int nnnm[] = {n,n,n,m};
  int mmnn[] = {m,m,n,n};
  int mmmnnn[] = {m,m,m,n,n,n};

  //* Creates distributed tensors initialized with zeros
  CTF_Tensor AS_A(4, nnnm, shapeTS4, dw);
  CTF_Tensor AS_B(6, mmmnnn, shapeAS6, dw);
  CTF_Tensor HS_B(6, mmmnnn, shapeHS6, dw);
  CTF_Tensor AS_C(4, mmnn, shapeAS4, dw);
  CTF_Tensor NS_A(4, nnnm, shapeNS4, dw);
  CTF_Tensor NS_B(6, mmmnnn, shapeTS6, dw);
  CTF_Tensor NS_C(4, mmnn, shapeTS4, dw);

#if DEBUG  >= 1
  if (rank == 0)
    printf("tensor creation succeed\n");
#endif

  //* Writes noise to local data based on global index
  srand48(2013);
  AS_A.get_local_data(&np, &indices, &pairs);
  for (i=0; i<np; i++ ) pairs[i] = drand48()-.5; //(1.E-3)*sin(indices[i]);
  AS_A.write_remote_data(np, indices, pairs);
  free(pairs);
  free(indices);
  AS_B.get_local_data(&np, &indices, &pairs);
  for (i=0; i<np; i++ ) pairs[i] = drand48()-.5; //(1.E-3)*sin(.33+indices[i]);
  AS_B.write_remote_data(np, indices, pairs);
  free(pairs);
  free(indices);
  AS_C.get_local_data(&np, &indices, &pairs);
  for (i=0; i<np; i++ ) pairs[i] = drand48()-.5; //(1.E-3)*sin(.66+indices[i]);
  AS_C.write_remote_data(np, indices, pairs);

  NS_A["abij"] -= AS_A["baij"];
  NS_A["abij"] += AS_A["abij"];

  HS_B["abcijk"] -= AS_B["abcjik"];
  HS_B["abcijk"] -= AS_B["abckji"];
  HS_B["abcijk"] -= AS_B["abcikj"];
  HS_B["abcijk"] += AS_B["abcijk"];
  HS_B["abcijk"] += AS_B["abckij"];
  HS_B["abcijk"] += AS_B["abcjki"];
  
  NS_B["ijkabc"] += HS_B["ijkabc"];
  NS_B["ikjabc"] -= HS_B["ijkabc"];
  NS_B["kjiabc"] -= HS_B["ijkabc"];
  
  NS_C["abij"] += AS_C["abij"];
  
  AS_C["abij"] += 0.5*AS_A["mnje"]*AS_B["abeimn"];
  
  NS_C["abij"] += 0.5*NS_A["mnje"]*NS_B["abeimn"];
  NS_C["abij"] -= NS_C["abji"];

  double nrm_AS = sqrt(AS_C.reduce(CTF_OP_SQNRM2));
  double nrm_NS = sqrt(NS_C.reduce(CTF_OP_SQNRM2));
#if DEBUG  >= 1
  if (rank == 0) printf("triangular norm of AS_C = %lf NS_C = %lf\n", nrm_AS, nrm_NS);
#endif
  nrm_AS = sqrt(AS_C["ijkl"]*AS_C["ijkl"]);
  nrm_NS = sqrt(NS_C["ijkl"]*NS_C["ijkl"]);
#if DEBUG  >= 1
  if (rank == 0) printf("norm of AS_C = %lf NS_C = %lf\n", nrm_AS, nrm_NS);
#endif
  AS_C["abij"] -= NS_C["abij"];
  
  double nrm = AS_C.reduce(CTF_OP_SQNRM2);
#if DEBUG  >= 1
  if (rank == 0){
    printf("norm of AS_C after contraction should be zero, is = %lf\n", nrm);
  }
#endif
  int pass = fabs(nrm) <= 1.E-6;

  if (rank == 0){
    if (pass)
      printf("{ AS_C[\"abij\"] += 0.5*AS_A[\"mnje\"]*AS_B[\"abeimn\"] } passed\n");
    else 
      printf("{ AS_C[\"abij\"] += 0.5*AS_A[\"mnje\"]*AS_B[\"abeimn\"] } failed\n");
  }

  free(pairs);
  free(indices);
  return pass;
} 

#ifndef TEST_SUITE

char* getCmdOption(char ** begin,
                   char ** end,
                   const   std::string & option){
  char ** itr = std::find(begin, end, option);
  if (itr != end && ++itr != end){
    return *itr;
  }
  return 0;
}


int main(int argc, char ** argv){
  int rank, np, niter, n, m;
  int const in_num = argc;
  char dir[120];
  char ** input_str = argv;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &np);

  if (getCmdOption(input_str, input_str+in_num, "-n")){
    n = atoi(getCmdOption(input_str, input_str+in_num, "-n"));
    if (n < 0) n = 4;
  } else n = 4;
  if (getCmdOption(input_str, input_str+in_num, "-m")){
    m = atoi(getCmdOption(input_str, input_str+in_num, "-m"));
    if (m < 0) m = 6;
  } else m = 6;

  if (getCmdOption(input_str, input_str+in_num, "-niter")){
    niter = atoi(getCmdOption(input_str, input_str+in_num, "-niter"));
    if (niter < 0) niter = 3;
  } else niter = 3;



  int pass = ccsdt_t3_to_t2(n, m, dir);
  assert(pass);


  MPI_Finalize();
  return 0;
}

#endif

