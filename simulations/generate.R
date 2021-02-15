library(rjson)

a <- list(1,2,3,4)
b <- list(10,20,30,40)
p <- list()
k <- 1
for (i in 1:4) {
  for (j in 1:4) {
     p[[k]] <- list(a=a[[i]], b=b[[j]])
     k <- k + 1
  }
}
j <- toJSON(p)
write(j, "params.json")
