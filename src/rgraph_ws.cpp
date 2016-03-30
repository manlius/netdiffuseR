 // [[Rcpp::depends(RcppArmadillo)]]
#include <RcppArmadillo.h>
#include "netdiffuser_extra.h"
using namespace Rcpp;

//' Ring lattice graph
//'
//' Creates a ring lattice with \eqn{n} vertices, each one of degree (at most) \eqn{k}
//' as an undirected graph. This is the basis of \code{\link{rgraph_ws}}.
//' @param n Integer scalar. Size of the graph.
//' @param k Integer scalar. Out-degree of each vertex.
//' @param undirected Logical scalar. Whether the graph is undirected or not.
//' @details when \code{undirected=TRUE}, the degree of each node always
//' even. So if \code{k=3}, then the degree will be \code{2}.
//' @return A sparse matrix of class \code{\link[Matrix:dgCMatrix-class]{dgCMatrix}} of size
//' \eqn{n\times n}{n * n}.
//' @references Watts, D. J., & Strogatz, S. H. (1998). Collective dynamics of
//' “small-world” networks. Nature, 393(6684), 440–2. \url{http://doi.org/10.1038/30918}
//' @export
//' @family simulation functions
// [[Rcpp::export]]
arma::sp_mat ring_lattice(int n, int k, bool undirected=false) {

  if ((n-1) < k)
    stop("k can be at most n - 1");

  arma::sp_mat graph(n,n);

  // Adjusting k
  if (undirected)
    if (k>1) k = (int) floor((double) k/2.0);

  // Connecting to k/2 next & previous neighbour
  for (int i=0;i<n;i++) {
    for (int j=1;j<=k;j++) {
      // Next neighbor
      int l = i+j;
      if (l >= n) l = l - n;

      graph.at(i,l) += 1.0;
      if (undirected) graph.at(l,i) += 1.0;
    }
  }
  return graph;
}

/** *R
library(Matrix)
library(sna)
x <- ring_lattice(6,2)
x
gplot(as.matrix(x), displaylabels = TRUE, mode="circle", jitter = FALSE)

# x <- ring_cpp(10,6)
# x
# gplot(as.matrix(x), displaylabels = TRUE, mode="circle", jitter = FALSE)
#
# x <- ring_cpp(7,7)
# x
# gplot(as.matrix(x), displaylabels = TRUE, mode="circle", jitter = FALSE)
*/

// [[Rcpp::export]]
arma::sp_mat rewire_graph_cpp(
    const arma::sp_mat & graph, double p,
    bool both_ends=false,
    bool self=false, bool multiple=false,
    bool undirected=false) {

  // Clonning graph
  arma::sp_mat newgraph(graph);
  int n = graph.n_cols;

  // Getting the indexes
  arma::umat indexes = sparse_indexes(graph);


  GetRNGstate();
  for (int i= 0;i<indexes.n_rows; i++) {

    // Checking user interrupt
    if (i % 1000 == 0)
      Rcpp::checkUserInterrupt();

    // Checking whether to change it or not
    if (unif_rand() > p) continue;

    // Indexes
    int j = indexes.at(i, 0);
    int k = indexes.at(i, 1);

    // Since it is undirected...
    if (undirected && (j < k) ) continue;

    // New end(s)
    int newk, newj;

    // If rewiring is in both sides, then we start from the left
    if (both_ends) newj = floor( (n-1)*unif_rand() );
    else newj = j;

    // Checking for conditions
    int wcount = 0;
    bool conditions = true;
    arma::uvec picked(n, arma::fill::zeros);

    while (conditions) {
      newk = floor( (n-1)*unif_rand() );

      // In the case that the individual actually is connected to everyone and
      // multiple is not allowed, this is needed to break out the loop
      if (picked.at(newk)) {
        if (++wcount >= n*n) break;
        continue;
      } else picked.at(newk) = 1;

      if (undirected && (newj < newk)) continue;

      // Self edges are not allowed, again, must check this on the new graph
      if (!self && newj == newk) continue;

      // Multiple edges are not allowed. Must check this on the new graph
      if (!multiple && (newgraph.at(newj, newk) != 0)) continue;

      conditions = false;
    }

    // Setting zeros
    double w = newgraph.at(j,k);
    newgraph.at(j,k) = 0;
    if (undirected) newgraph.at(k,j) = 0;

    // Adding up
    newgraph.at(newj,newk) += w;
    if (undirected) newgraph.at(newk,newj) += w;

  }
  PutRNGstate();

  return newgraph;
}


/** *R
rgraph_ws <- function(n,k,p, both_ends=FALSE, self=FALSE, multiple=FALSE) {
  rewire_graph_cpp(ring_lattice(n, k), p, both_ends,
                   self, multiple, true)
}

x <- ring_lattice(14, 2)
gplot(as.matrix(x), mode="circle", jitter=FALSE, usecurve = TRUE, gmode = "graph")
gplot(as.matrix(netdiffuseR:::rewire_graph_cpp(x, .1)), mode="circle", jitter=FALSE, usecurve = TRUE, gmode = "graph")
gplot(as.matrix(netdiffuseR:::rewire_graph_cpp(x, 1)), mode="circle", jitter=FALSE, usecurve = TRUE, gmode = "graph")
*/
