/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 2012 Torsten Hueter, torstenhtr <at> gmx.de
 * Copyright (C) 2012 Kicad Developers, see change_log.txt for contributors.
 *
 * Graphics Abstraction Layer (GAL) - base class
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <wx/log.h>

#include <gal/graphics_abstraction_layer.h>
#include <gal/definitions.h>

#include <cmath>

using namespace KIGFX;


const double GAL::METRIC_UNIT_LENGTH = 1e9;


GAL::GAL() :
    strokeFont( this )
{
    // Set the default values for the internal variables
    SetIsFill( false );
    SetIsStroke( true );
    SetFillColor( COLOR4D( 0.0, 0.0, 0.0, 0.0 ) );
    SetStrokeColor( COLOR4D( 1.0, 1.0, 1.0, 1.0 ) );
    SetLookAtPoint( VECTOR2D( 0, 0 ) );
    SetZoomFactor( 1.0 );
    SetWorldUnitLength( 1.0 / METRIC_UNIT_LENGTH * 2.54 );   // 1 inch in nanometers
    SetScreenDPI( 106 );                                     // Display resolution setting
    SetDepthRange( VECTOR2D( GAL::MIN_DEPTH, GAL::MAX_DEPTH ) );
    SetLayerDepth( 0.0 );
    SetFlip( false, false );
    SetLineWidth( 1.0 );
    computeWorldScale();

    // Set grid defaults
    SetGridVisibility( true );
    SetGridStyle( GRID_STYLE_LINES );
    SetGridDrawThreshold( 10 );
    SetCoarseGrid( 10 );
    SetGridLineWidth( 0.5 );

    // Initialize the cursor shape
    SetCursorColor( COLOR4D( 1.0, 1.0, 1.0, 1.0 ) );
    SetCursorSize( 80 );
    SetCursorEnabled( false );

    strokeFont.LoadNewStrokeFont( newstroke_font, newstroke_font_bufsize );
}


GAL::~GAL()
{
}


void GAL::SetTextAttributes( const EDA_TEXT* aText )
{
    SetGlyphSize( VECTOR2D( aText->GetSize() ) );
    SetHorizontalJustify( aText->GetHorizJustify() );
    SetVerticalJustify( aText->GetVertJustify() );
    SetFontBold( aText->IsBold() );
    SetFontItalic( aText->IsItalic() );
    SetTextMirrored( aText->IsMirrored() );
}

VECTOR2D GAL::GetTextLineSize( const UTF8& aText ) const
{
    // Compute the X and Y size of a given text.
    // Because computeTextLineSize expects a one line text,
    // aText is expected to be only one line text.
    return strokeFont.computeTextLineSize( aText );
}


void GAL::ComputeWorldScreenMatrix()
{
    computeWorldScale();

    worldScreenMatrix.SetIdentity();

    MATRIX3x3D translation;
    translation.SetIdentity();
    translation.SetTranslation( 0.5 * VECTOR2D( screenSize ) );

    MATRIX3x3D scale;
    scale.SetIdentity();
    scale.SetScale( VECTOR2D( worldScale, worldScale ) );

    MATRIX3x3D flip;

    flip.SetIdentity();
    flip.SetScale( VECTOR2D( globalFlipX ? -1.0 : 1.0, globalFlipY ? -1.0 : 1.0 ) );

    MATRIX3x3D lookat;
    lookat.SetIdentity();
    lookat.SetTranslation( -lookAtPoint );

    worldScreenMatrix = translation * flip * scale * lookat * worldScreenMatrix;
    screenWorldMatrix = worldScreenMatrix.Inverse();
}


void GAL::DrawGrid()
{
    if( !gridVisibility )
        return;

    SetTarget( TARGET_NONCACHED );

    // Draw the grid
    // For the drawing the start points, end points and increments have
    // to be calculated in world coordinates
    VECTOR2D worldStartPoint = screenWorldMatrix * VECTOR2D( 0.0, 0.0 );
    VECTOR2D worldEndPoint   = screenWorldMatrix * VECTOR2D( screenSize );

    int gridScreenSizeDense  = KiROUND( gridSize.x * worldScale );
    int gridScreenSizeCoarse = KiROUND( gridSize.x * static_cast<double>( gridTick ) * worldScale );

    // Compute the line marker or point radius of the grid
    double marker = 2.0 * gridLineWidth / worldScale;
    double doubleMarker = 2.0 * marker;

    // Check if the grid would not be too dense
    if( std::max( gridScreenSizeDense, gridScreenSizeCoarse ) > gridDrawThreshold )
    {
        // Compute grid variables
        int gridStartX  = KiROUND( worldStartPoint.x / gridSize.x );
        int gridEndX    = KiROUND( worldEndPoint.x / gridSize.x );
        int gridStartY  = KiROUND( worldStartPoint.y / gridSize.y );
        int gridEndY    = KiROUND( worldEndPoint.y / gridSize.y );

        // Correct the index, else some lines are not correctly painted

        gridStartY -= std::abs( gridOrigin.y / gridSize.y ) + 1;
        gridEndY += std::abs( gridOrigin.y / gridSize.y ) + 1;

        if ( gridStartX <= gridEndX )
        {
            gridStartX -= std::abs( gridOrigin.x / gridSize.x ) + 1;
            gridEndX += std::abs( gridOrigin.x / gridSize.x ) + 1;
        }
        else
        {
            gridStartX += std::abs( gridOrigin.x / gridSize.x ) + 1;
            gridEndX -= std::abs( gridOrigin.x / gridSize.x ) + 1;
        }

        int dirX = gridEndX >= gridStartX ? 1 : -1;
        int dirY = gridEndY >= gridStartY ? 1 : -1;

        // Draw the grid behind all other layers
        SetLayerDepth( depthRange.y * 0.75 );

        if( gridStyle == GRID_STYLE_LINES )
        {
            SetIsFill( false );
            SetIsStroke( true );
            SetStrokeColor( gridColor );

            // Now draw the grid, every coarse grid line gets the double width

            // Vertical lines
            for( int j = gridStartY; j != gridEndY; j += dirY )
            {
                if( j % gridTick == 0 && gridScreenSizeDense > gridDrawThreshold )
                    SetLineWidth( doubleMarker );
                else
                    SetLineWidth( marker );

                if( ( j % gridTick == 0 && gridScreenSizeCoarse > gridDrawThreshold )
                    || gridScreenSizeDense > gridDrawThreshold )
                {
                    drawGridLine( VECTOR2D( gridStartX * gridSize.x, j * gridSize.y + gridOrigin.y ),
                                  VECTOR2D( gridEndX * gridSize.x,   j * gridSize.y + gridOrigin.y ) );
                }
            }

            // Horizontal lines
            for( int i = gridStartX; i != gridEndX; i += dirX )
            {
                if( i % gridTick == 0 && gridScreenSizeDense > gridDrawThreshold )
                    SetLineWidth( doubleMarker );
                else
                    SetLineWidth( marker );

                if( ( i % gridTick == 0 && gridScreenSizeCoarse > gridDrawThreshold )
                    || gridScreenSizeDense > gridDrawThreshold )
                {
                    drawGridLine( VECTOR2D( i * gridSize.x + gridOrigin.x, gridStartY * gridSize.y ),
                                  VECTOR2D( i * gridSize.x + gridOrigin.x, gridEndY * gridSize.y ) );
                }
            }
        }
        else    // Dotted grid
        {
            bool tickX, tickY;
            SetIsFill( true );
            SetIsStroke( false );
            SetFillColor( gridColor );

            for( int j = gridStartY; j != gridEndY; j += dirY )
            {
                if( j % gridTick == 0 && gridScreenSizeDense > gridDrawThreshold )
                    tickY = true;
                else
                    tickY = false;

                for( int i = gridStartX; i != gridEndX; i += dirX )
                {
                    if( i % gridTick == 0 && gridScreenSizeDense > gridDrawThreshold )
                        tickX = true;
                    else
                        tickX = false;

                    if( tickX || tickY || gridScreenSizeDense > gridDrawThreshold )
                    {
                        double radius = ( tickX && tickY ) ? doubleMarker : marker;
                        DrawRectangle( VECTOR2D( i * gridSize.x - radius + gridOrigin.x,
                                                 j * gridSize.y - radius + gridOrigin.y ),
                                       VECTOR2D( i * gridSize.x + radius + gridOrigin.x,
                                                 j * gridSize.y + radius + gridOrigin.y ) );
                    }
                }
            }
        }
    }
}


VECTOR2D GAL::GetGridPoint( const VECTOR2D& aPoint ) const
{
    return VECTOR2D( KiROUND( ( aPoint.x - gridOffset.x ) / gridSize.x ) * gridSize.x + gridOffset.x,
                     KiROUND( ( aPoint.y - gridOffset.y ) / gridSize.y ) * gridSize.y + gridOffset.y );
}

const int GAL::MIN_DEPTH = -1024;
const int GAL::MAX_DEPTH = 1023;
const int GAL::GRID_DEPTH = MAX_DEPTH - 1;
