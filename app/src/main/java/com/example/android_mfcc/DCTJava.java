package com.example.android_mfcc;

import static java.lang.Math.PI;
import static java.lang.Math.cos;
import static java.lang.Math.sqrt;

public class DCTJava {

    /** @brief constructor. it pre-calculates a table.
     *
     * @param numPoints : number of points in the input.
     */
    DCTJava ( int numPoints ) {
        mNumPoints = numPoints;
        makeDCTTable();
    }

    /** @brief main function for DCT
     *
     * @param array_in : time domain samples
     * @return : frequency domain samples
     */
    double[] transform(double[] array_in) {

        double[] array_out = new double[mNumPoints + 1];

        for ( int i = 0; i < mNumPoints + 1 ; i++ ) {
            double val = 0.0;
            for ( int j = 0; j < mNumPoints; j++ ) {

                val += mDCTTable[i][j] * array_in[j];
            }
            array_out[i] = val;
        }

        return array_out;
    }

    private void makeDCTTable() {

        mDCTTable = new double[ mNumPoints + 1][ mNumPoints ];

        final double C = sqrt( 2.0 / mNumPoints );

        for ( int i = 0; i <= mNumPoints; i++ ) {

            for (int j = 0; j < mNumPoints; j++ ) {

                final double di = (double)i ;
                final double dj = (double)j + 0.5 ;
                mDCTTable[i][j] = C * cos( PI * di * dj / (double)mNumPoints );
            }
        }
    }

    private int        mNumPoints;
    private double[][] mDCTTable;
}
