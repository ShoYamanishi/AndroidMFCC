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
    float[] transform(float[] array_in) {

        float[] array_out = new float[mNumPoints + 1];
        for ( int i = 0; i < mNumPoints + 1 ; i++ ) {
            float val = 0.0f;
            for ( int j = 0; j < mNumPoints; j++ ) {

                val += mDCTTable[i][j] * array_in[j];
            }
            array_out[i] = val;
        }

        return array_out;
    }

    private void makeDCTTable() {

        mDCTTable = new float[ mNumPoints + 1][ mNumPoints ];

        final float C = (float)sqrt( 2.0 / ((double)mNumPoints) );

        for ( int i = 0; i <= mNumPoints; i++ ) {

            for (int j = 0; j < mNumPoints; j++ ) {

                final float di = (float)i ;
                final float dj = (float)j + 0.5f ;
                mDCTTable[i][j] = C * (float)cos( PI * di * dj / (float)mNumPoints );
            }
        }
    }

    private int        mNumPoints;
    private float[][] mDCTTable;
}
