/*
 * capture.h
 *
 *  Created on: Oct 1, 2013
 *      Author: leizhao
 */

#ifndef CAPTURE_H_
#define CAPTURE_H_

void capR_start_capturing();
void capR_stop_capturing();
void capR_capture_primitive(SEXP, SEXP, SEXP);

#endif /* CAPTURE_H_ */
