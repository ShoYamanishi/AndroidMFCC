package com.example.android_mfcc;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Log;

import androidx.appcompat.widget.AppCompatImageView;

import static android.graphics.Bitmap.Config.*;
import static java.lang.Math.max;
import static java.lang.Math.min;

import java.time.Instant;
import java.util.Random;
import java.util.Timer;
import java.util.TimerTask;

public class ScrollingHeatMapView extends AppCompatImageView {

    private static final String TAG = ScrollingHeatMapView.class.getSimpleName();

    public ScrollingHeatMapView( Context context ) {
        super( context );
    }

    public ScrollingHeatMapView( Context context, AttributeSet attrs ) {
        super( context, attrs );
    }

    public ScrollingHeatMapView( Context context, AttributeSet attrs, int defStyleAttr ) {
        super( context, attrs, defStyleAttr );
    }

    @Override
    protected void onDraw(Canvas canvas) {
        canvas.save();
        super.onDraw(canvas);

        double scaleRatio = (double)canvas.getWidth() / (double) mResW;

        if ( mRender1to2 ) {

            Rect rectSrc1 = new Rect(
                    mRenderOffset,
                    0,
                    mResW,
                    mResH
            );

            Rect rectSrc2 = new Rect(
                    0,
                    0,
                    mRenderOffset,
                    mResH
            );

            Rect rectDst1 = new Rect(
                    0,
                    0,
                    (int) (scaleRatio * (double)(mResW - mRenderOffset)),
                    canvas.getHeight()
            );

            Rect rectDst2 = new Rect(
                    (int) ( scaleRatio * (double)(mResW - mRenderOffset)),
                    0,
                    canvas.getWidth(),
                    canvas.getHeight()
            );

            canvas.drawBitmap( mBitmapRender1, rectSrc1, rectDst1, null );
            canvas.drawBitmap( mBitmapRender2, rectSrc2, rectDst2, null );
        }
        else {

            Rect rectSrc1 = new Rect(
                    mRenderOffset,
                    0,
                    mResW,
                    mResH
            );

            Rect rectSrc2 = new Rect(
                    0,
                    0,
                    mRenderOffset,
                    mResH
            );

            Rect rectDst1 = new Rect(
                    0,
                    0,
                    (int) (scaleRatio * (double)(mResW - mRenderOffset)),
                    canvas.getHeight()
            );

            Rect rectDst2 = new Rect(
                    (int) ( scaleRatio * (double)(mResW - mRenderOffset)),
                    0,
                    canvas.getWidth(),
                    canvas.getHeight()
            );

            canvas.drawBitmap( mBitmapRender2, rectSrc1, rectDst1, null );
            canvas.drawBitmap( mBitmapRender1, rectSrc2, rectDst2, null );

        }

        canvas.restore();
    }

    public void setResolution( int w, int h ) {

        mResW = w;
        mResH = h;

        mBitmapRender1 = Bitmap.createBitmap( w, h, ARGB_8888 );
        mBitmapRender2 = Bitmap.createBitmap( w, h, ARGB_8888 );

        Canvas canvas1 = new Canvas( mBitmapRender1 );
        canvas1.drawColor( 0xff000000 );

        Canvas canvas2 = new Canvas( mBitmapRender2 );
        canvas2.drawColor( 0xff000000 );

        mRender1to2   = true;
        mRenderOffset = 0;

    }

    public void setColorType(int t) { mColorType = t; }

    public void setNewColumn(double[] vec) {

        if ( mRender1to2 ) {

            for ( int i = 0 ; i < vec.length ; i++ ) {
                mBitmapRender2.setPixel( mRenderOffset, mResH - 1 - i, heatColor( vec[i] ) );
            }

            mRenderOffset++;
            if (mRenderOffset == mResW) {
                mRender1to2 = false;
                mRenderOffset = 0;
            }

        }
        else {
            for ( int i = 0 ; i < vec.length ; i++ ) {
                mBitmapRender1.setPixel( mRenderOffset, mResH -1 - i, heatColor( vec[i] ) );
            }

            mRenderOffset++;
            if (mRenderOffset == mResW) {
                mRender1to2 = true;
                mRenderOffset = 0;
            }
        }

        invalidate();
    }

    /**
     * heapmap: (rgb) (0,0,0) -> (0,0,1) -> (0,1,1) -> (0,1,0) -> (1,1,0) -> (1,0,0) -> (1,1,1)
     * @param v : normalized value in [0.0, 1.0]
     * @return color in heat map
     */
    int heatColor(double v) {
        if ( mColorType == 0 ) {
            return heatColorRGB(v);
        }
        else {
            return heatColorMonoColorLimeGreen(v);
        }
    }


    int heatColorRGB(double v) {
        int A = 255;
        int R = 0;
        int G = 0;
        int B = 0;

        double v6 = Math.max(Math.min(v, 1.0), 0.0) * 6.0;
        //Log.i(TAG, "V:" + String.valueOf(v6));
        if (0.0 <= v6 && v6 < 1.0) {
            B = (int) (v6 * 255.0);
        } else if (1.0 <= v6 && v6 < 2.0) {
            G = (int) ((v6 - 1.0) * 255.0);
            B = 255;
        } else if (2.0 <= v6 && v6 < 3.0) {
            G = 255;
            B = (int) ((3.0 - v6) * 255.0);
        } else if (3.0 <= v6 && v6 < 4.0) {
            R = (int) ((v6 - 3.0) * 255.0);
            G = 255;
        } else if (4.0 <= v6 && v6 < 5.0) {
            R = 255;
            G = (int) ((5.0 - v6) * 255.0);
        } else {
            R = 255;
            G = (int) ((v6 - 5.0) * 255.0);
            B = (int) ((v6 - 5.0) * 255.0);
        }
        int color = (A & 0xff) << 24 | (R & 0xff) << 16 | (G & 0xff) << 8 | (B & 0xff);
        //Log.i(TAG, "color: R" + String.valueOf(R) + " G " + String.valueOf(G) + " B " + String.valueOf(B));
        return color;
    }


    int heatColorMonoColorLimeGreen(double v)
    {

        int intensity  = (int) (max(min(v,1.0), 0.0)  * 256.0);
        int A = 255;
        int B = intensity;
        int G = intensity;
        int R = 0;

        int color = (A & 0xff) << 24 | (R & 0xff) << 16 | (G & 0xff) << 8 | (B & 0xff);
        //Log.i(TAG, "color: R" + String.valueOf(R) + " G " + String.valueOf(G) + " B " + String.valueOf(B));
        return color;

    }


    int     mResW;
    int     mResH;
    Bitmap  mBitmapRender1;
    Bitmap  mBitmapRender2;

    boolean mRender1to2;
    int     mRenderOffset;

    int     mColorType = 0;

}
