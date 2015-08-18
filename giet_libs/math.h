// Avril 2015
//  importation de fonctions mathématiques de la librairie ulibc-0.9.32 pour implémentations sur GIET
//  Pierre LAURENT

#ifndef MATH_H_
#define MATH_H_

double	fabs	(double x);
double	floor	(double x);
double	sin		(double x);
double	cos		(double x);
double	pow		(double x, double y);
int		isnan	(double x);
int		isfinite(double x);
double	scalbln	(double x, long n);
double	scalbn	(double x, int n);
double	copysign(double x, double y);
double 	rint	(double x);
double	sqrt	(double x);

#endif
