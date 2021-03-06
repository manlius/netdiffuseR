Notes on the EpiModel R package
================
George G. Vega Yon
November 1, 2017

Introduction
============

The following document presents a set of notes from "EpiModel: An R Package for Mathematical Modeling of Infectious Disease over Networks".

Another interesting example is provided [here](http://statnet.github.io/tut/NewNet.html#example_2:_migration)

The process is as follows:

1.  Initialize and set the network calling `network::network.initialize`,

2.  Estimate the network parameters calling `netest` passing network effects, this will be used to simulate the dynamic network, and initialize the simulation calling `init.net` (initial infected, recovered, etc.)

3.  Define the set of parameters that will be used during the simulation using `param.net`

4.  Define the control functions for the simulation (the modules) using `control.net`

5.  Run your simulation by calling `netsim` and passing your estimates from `netest`, the output from `param.net` and from `control.net`

Internal work of `netsim` from `EpiModel`

``` r
# Loop through steps
for (at in max(2, control$start):control$nsteps) {

  # Fetching the ordering in which modules happen
  morder <- control$module.order
  if (is.null(morder)) {
    lim.bi.mods <- control$bi.mods[-which(control$bi.mods %in% 
      c("initialize.FUN", "verbose.FUN"))]
    morder <- c(control$user.mods, lim.bi.mods)
  }
  
  # Applying the models (here is where the simulation actually happens)
  for (i in seq_along(morder)) {
    dat <- do.call(control[[morder[i]]], list(dat, 
      at))
  }
  
  # Printing results on screen
  if (!is.null(control[["verbose.FUN"]])) {
    do.call(control[["verbose.FUN"]], list(dat, 
      type = "progress", s, at))
  }
}
```

Extended Example
================

In this section, I review some of the examples presented in the document. The subsections are titled as the subsection in the original document. We first need to load the R packages that we will be used.

``` r
library(EpiModel)
library(sna)
library(networkDynamic)
```

5.2. Example 1: Age-dependent mortality extension
-------------------------------------------------

The following lines of code introduce a new module that sets the mortality rate as a function of age (which is not included by default)

``` r
aging <- function(dat, at) {
  
  if (at == 2) {
    # Initializing age  
    n <- sum(dat$attr$active == 1)
    dat$attr$age <- sample(
      seq(from = 18, to = 69 + 11 / 12, by = 1 / 12),
      n, replace = TRUE)
    
  } else {
    # Increasing age
    dat$attr$age <- dat$attr$age + 1 / 12
    
  }
  
  # Saving some stats: Average age
  if (at == 2) {
    dat$epi$meanAge <- c(NA, mean(dat$attr$age, na.rm = TRUE))
  } else {
    dat$epi$meanAge[at] <- mean(dat$attr$age, na.rm = TRUE)
  }
  
  return(dat)
}
```

This example shows how to create a new mortality extension, which is to replace the default function passed to `control.net` as the argument `deaths.FUN`. Currently the default is `deaths.net`.

``` r
ages        <- 18:69
death.rates <- 1/(70*12 - ages*12)


dfunc <- function(dat, at) {
  
  # Finding active nodes
  idsElig <- which(dat$attr$active == 1)
  nElig   <- length(idsElig)
  nDeaths <- 0
  
  # If there are active nodes
  if (nElig > 0) {
    
    # Fetching their ages (from attr)
    ages    <- dat$attr$age[idsElig]
    
    # The maximun age is fetched from params
    max.age <- dat$param$max.age
    
    death.rates <- pmin(1, 1 / (max.age * 12 - ages * 12))
    vecDeaths <- which(rbinom(nElig, 1, death.rates) == 1)
    idsDeaths <- idsElig[vecDeaths]
    nDeaths <- length(idsDeaths)
    
    # If there are deads, then:
    #   - Set them as inactive,
    #   - Set the exit time,
    #   - And deactivate the vertices (and edges) in the network
    if (nDeaths > 0) {
      
      dat$attr$active[idsDeaths] <- 0
      dat$attr$exitTime[idsDeaths] <- at
      
      dat$nw <- deactivate.vertices(
        dat$nw,
        onset    = at,
        terminus = Inf,
        v        = idsDeaths,
        deactivate.edges = TRUE
      )
      
    }
  }
  
  # Adding summary stats
  if (at == 2) {
    # In the first run, the statistic d.flow is created
    dat$epi$d.flow <- c(0, nDeaths)
  } else {
    # Then it is just filled
    dat$epi$d.flow[at] <- nDeaths
  }
  
  return(dat)
}
```

The following module replaces the `birth.FUN` (which by default is `births.net`):

``` r
bfunc <- function(dat, at) {
  
  # Getting some parameters
  growth.rate <- dat$param$growth.rate
  exptPopSize <- dat$epi$num[1] * (1 + growth.rate * at)
  n           <- network.size(dat$nw)
  numNeeded   <- exptPopSize - sum(dat$attr$active == 1)
  
  # If new births are needed, then simulate them using a poisson
  if (numNeeded > 0) {
    nBirths <- rpois(1, numNeeded)
  } else {
    nBirths <- 0
  }
  
  # If there were new birdths, then add them to the network and 
  # activate them
  if (nBirths > 0) {
    dat$nw   <- add.vertices(dat$nw, nv = nBirths)
    newNodes <- (n + 1):(n + nBirths)
    dat$nw   <- activate.vertices(
      dat$nw, onset = at, terminus = Inf, v = newNodes
      )
  }
  
  # Initializing vertices atributes (if any)
  if (nBirths > 0) {
    dat$attr$active  <- c(dat$attr$active, rep(1, nBirths))
    dat$attr$status  <- c(dat$attr$status, rep("s", nBirths))
    dat$attr$infTime <- c(dat$attr$infTime, rep(NA, nBirths))
    dat$attr$age     <- c(dat$attr$age, rep(18, nBirths))
  }
  
  # Saving stats
  if (at == 2) {
    dat$epi$b.flow <- c(0, nBirths)
  } else {
    dat$epi$b.flow[at] <- nBirths
  }
  
  return(dat)
}
```

The following new module computes and stores the mean degree through the simulation, notice that in the case of `networkDynamic`, the time starts at 0.

``` r
dfun <- function(dat, at) {
  
  if (at == 2) {
    dat$epi$meandeg <- c(0, mean(degree(network.extract(dat$nw, at = at - 1L))))
  } else {
    dat$epi$meandeg[at] <- mean(degree(network.extract(dat$nw, at = at - 1L)))
  }
  
  dat
}
```

``` r
# Step 1: Bernoulli network
nw <- network::network.initialize(500, directed = FALSE)

# Step 2
est3 <- netest(
  nw, formation = ~edges, target.stats = 150,
  coef.diss = dissolution_coefs(~offset(edges), 60, mean(death.rates))
  )

# Step 3: Passing the parameters
param <- param.net(inf.prob = 0.15, growth.rate = 0.01/12, max.age = 70)
init <- init.net(i.num = 50)

# Step 4: Passing the new functions
control <- control.net(
  type = "SI", nsims = 1, nsteps = 100, deaths.FUN = dfunc, births.FUN = bfunc,
  aging.FUN = aging, depend = TRUE, verbose = FALSE,
  
  # This function is new in -control.net-
  deg.FUN = dfun
  )

# Step 5: Running the simulation
sim3 <- netsim(est3, param, init, control)
```

``` r
# If our simulation worked, then we should be able to replicate the
# mean degree
degs <- sapply(1:100, function(i) {
  mean(degree(network.extract(sim3$network$sim1, at = i-1)))
})
cor(
  sim3$epi$meandeg[-1,1],
  degs[-1]
  ) # This should be one!
```

    ## [1] 1

5.3. Example 2: SEIR epidemic model
-----------------------------------

In this example the authors describe the a small set of modifications that can be done to the function `infection.net` so that there's an intermediate step in the process, exposed, the initial stage of the infection.

The code of `infection.net` follows:

``` r
infection.net <- function (dat, at) 
{
    # Vector of attributes
    active <- dat$attr$active
    status <- dat$attr$status
    
    # Parameters of the network
    modes <- dat$param$modes
    mode <- idmode(dat$nw)
    
    inf.prob <- dat$param$inf.prob
    inf.prob.m2 <- dat$param$inf.prob.m2
    act.rate <- dat$param$act.rate
    
    # The actual network
    nw <- dat$nw
    tea.status <- dat$control$tea.status
    
    # Which ids are susceptible and infected?
    idsSus <- which(active == 1 & status == "s")
    idsInf <- which(active == 1 & status == "i")
    
    # How many are active?
    nActive <- sum(active == 1)
    nElig <- length(idsInf)
    nInf <- nInfM2 <- totInf <- 0
    
    # If there are elegible, then...
    if (nElig > 0 && nElig < nActive) {
      
        # Finding edges in which there are inf and suscept
        # this returns a data frame with ids of infect and suscept.
        del <- discord_edgelist(dat, idsInf, idsSus, at)
        
        # If there are any
        if (!(is.null(del))) {
          
            # How long have the infected been infected
            # This is relevant as the probability of infecting is a function
            # of this time
            del$infDur <- at - dat$attr$infTime[del$inf]
            del$infDur[del$infDur == 0] <- 1
            linf.prob <- length(inf.prob)
            
            # Specifying infection probabilities from mode 1 to 2 (if not specified)
            if (is.null(inf.prob.m2)) {
              
                del$transProb <- ifelse(del$infDur <= linf.prob, 
                  inf.prob[del$infDur], inf.prob[linf.prob])
                
            } # Otherwise, specify these according to the type of edge
            else {
              
                del$transProb <- ifelse(del$sus <= nw %n% "bipartite", 
                  ifelse(del$infDur <= linf.prob, inf.prob[del$infDur], 
                    inf.prob[linf.prob]), ifelse(del$infDur <= 
                    linf.prob, inf.prob.m2[del$infDur], inf.prob.m2[linf.prob]))
                
            }
            if (!is.null(dat$param$inter.eff) && at >= dat$param$inter.start) {
              
                del$transProb <- del$transProb * (1 - dat$param$inter.eff)
                
            }
            
            lact.rate <- length(act.rate)
            del$actRate <- ifelse(del$infDur <= lact.rate, act.rate[del$infDur], 
                act.rate[lact.rate])
            del$finalProb <- 1 - (1 - del$transProb)^del$actRate
            transmit <- rbinom(nrow(del), 1, del$finalProb)
            del <- del[which(transmit == 1), ]
            idsNewInf <- unique(del$sus)
            nInf <- sum(mode[idsNewInf] == 1)
            nInfM2 <- sum(mode[idsNewInf] == 2)
            totInf <- nInf + nInfM2
            if (totInf > 0) {
                if (tea.status == TRUE) {
                  nw <- activate.vertex.attribute(nw, prefix = "testatus", 
                    value = "i", onset = at, terminus = Inf, 
                    v = idsNewInf)
                }
                dat$attr$status[idsNewInf] <- "i"
                dat$attr$infTime[idsNewInf] <- at
                form <- get_nwparam(dat)$formation
                fterms <- get_formula_terms(form)
                if ("status" %in% fterms) {
                  nw <- set.vertex.attribute(nw, "status", dat$attr$status)
                }
            }
            if (any(names(nw$gal) %in% "vertex.pid")) {
                del$sus <- get.vertex.pid(nw, del$sus)
                del$inf <- get.vertex.pid(nw, del$inf)
            }
        }
    }
    if (totInf > 0) {
        del <- del[!duplicated(del$sus), ]
        if (at == 2) {
            dat$stats$transmat <- del
        }
        else {
            dat$stats$transmat <- rbind(dat$stats$transmat, del)
        }
    }
    if (at == 2) {
        dat$epi$si.flow <- c(0, nInf)
        if (modes == 2) {
            dat$epi$si.flow.m2 <- c(0, nInfM2)
        }
    }
    else {
        dat$epi$si.flow[at] <- nInf
        if (modes == 2) {
            dat$epi$si.flow.m2[at] <- nInfM2
        }
    }
    dat$nw <- nw
    return(dat)
}
```

Own example
===========

``` r
contagion <- function(dat, at) {
  
  
  
}
```
