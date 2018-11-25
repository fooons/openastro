/*****************************************************************************
 *
 * demosaicSettings.cc -- class for the demosaic settings in the settings UI
 *
 * Copyright 2013,2014,2015,2016,2018
 *     James Fidell (james@openastroproject.org)
 *
 * License:
 *
 * This file is part of the Open Astro Project.
 *
 * The Open Astro Project is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The Open Astro Project is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Open Astro Project.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <oa_common.h>

extern "C" {
#include <openastro/demosaic.h>
#include <openastro/video/formats.h>
}

#include "trampoline.h"
#include "captureSettings.h"
#include "demosaicSettings.h"
#include "fitsSettings.h"


DemosaicSettings::DemosaicSettings ( QWidget* parent, demosaicConfig* dConf,
		int demosaic, trampolineFuncs* redirs ) : QWidget ( parent ),
		trampolines ( redirs ), demosaicOpts ( demosaic ), pconfig ( dConf )
{
	if ( demosaicOpts ) {
    demosaicLabel = new QLabel ( tr ( "When demosaic is enabled:" ));
    previewBox = new QCheckBox ( tr ( "Demosaic preview image" ), this );
    previewBox->setChecked ( pconfig->demosaicPreview );
    outputBox = new QCheckBox ( tr ( "Demosaic output data" ), this );
    outputBox->setChecked ( pconfig->demosaicOutput );
	}

  cfaLabel = new QLabel ( tr ( "Bayer format" ));
  cfaButtons = new QButtonGroup ( this );
  rggbButton = new QRadioButton ( tr ( "RGGB" ));
  bggrButton = new QRadioButton ( tr ( "BGGR" ));
  grbgButton = new QRadioButton ( tr ( "GRBG" ));
  gbrgButton = new QRadioButton ( tr ( "GBRG" ));
  cmygButton = new QRadioButton ( tr ( "CMYG" ));
  mcgyButton = new QRadioButton ( tr ( "MCGY" ));
  ygcmButton = new QRadioButton ( tr ( "YGCM" ));
  gymcButton = new QRadioButton ( tr ( "GYMC" ));
  autoButton = new QRadioButton ( tr ( "Auto" ));
  rggbButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_RGGB ? 1 : 0 );
  bggrButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_BGGR ? 1 : 0 );
  grbgButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_GRBG ? 1 : 0 );
  gbrgButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_GBRG ? 1 : 0 );
  cmygButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_CMYG ? 1 : 0 );
  mcgyButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_MCGY ? 1 : 0 );
  ygcmButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_YGCM ? 1 : 0 );
  gymcButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_GYMC ? 1 : 0 );
  autoButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_AUTO ? 1 : 0 );
  rggbButton->setIcon ( QIcon ( ":/qt-icons/RGGB.png" ));
  bggrButton->setIcon ( QIcon ( ":/qt-icons/BGGR.png" ));
  grbgButton->setIcon ( QIcon ( ":/qt-icons/GRBG.png" ));
  gbrgButton->setIcon ( QIcon ( ":/qt-icons/GBRG.png" ));
  cmygButton->setIcon ( QIcon ( ":/qt-icons/CMYG.png" ));
  mcgyButton->setIcon ( QIcon ( ":/qt-icons/MCGY.png" ));
  ygcmButton->setIcon ( QIcon ( ":/qt-icons/YGCM.png" ));
  gymcButton->setIcon ( QIcon ( ":/qt-icons/GYMC.png" ));

  cfaButtons->addButton ( rggbButton );
  cfaButtons->addButton ( bggrButton );
  cfaButtons->addButton ( grbgButton );
  cfaButtons->addButton ( gbrgButton );
  cfaButtons->addButton ( cmygButton );
  cfaButtons->addButton ( mcgyButton );
  cfaButtons->addButton ( ygcmButton );
  cfaButtons->addButton ( gymcButton );
  cfaButtons->addButton ( autoButton );

  methodLabel = new QLabel ( tr ( "Demosaic method" ));
  methodButtons = new QButtonGroup ( this );
  nnButton = new QRadioButton ( tr ( "Nearest Neighbour" ));
  bilinearButton = new QRadioButton ( tr ( "Bilinear" ));
  smoothHueButton = new QRadioButton ( tr ( "Smooth Hue" ));
  vngButton = new QRadioButton ( tr ( "VNG" ));
  nnButton->setChecked ( pconfig->demosaicMethod ==
      OA_DEMOSAIC_NEAREST_NEIGHBOUR ? 1 : 0 );
  bilinearButton->setChecked ( pconfig->demosaicMethod ==
      OA_DEMOSAIC_BILINEAR ? 1 : 0 );
  smoothHueButton->setChecked ( pconfig->demosaicMethod ==
      OA_DEMOSAIC_SMOOTH_HUE ? 1 : 0 );
  vngButton->setChecked ( pconfig->demosaicMethod ==
      OA_DEMOSAIC_VNG ? 1 : 0 );

  methodButtons->addButton ( nnButton );
  methodButtons->addButton ( bilinearButton );
  methodButtons->addButton ( smoothHueButton );
  methodButtons->addButton ( vngButton );

  monoAsRaw = new QCheckBox ( tr ( "Treat mono frame as raw colour" ), this );
  monoAsRaw->setChecked ( pconfig->monoIsRawColour );

  hbox = new QHBoxLayout ( this );
  lbox = new QVBoxLayout();
  rbox = new QVBoxLayout();
	if ( demosaicOpts ) {
    lbox->addWidget ( demosaicLabel );
    lbox->addWidget ( previewBox );
    lbox->addWidget ( outputBox );
    lbox->addSpacing ( 25 );
	}
  rbox->addWidget ( cfaLabel );
  rbox->addWidget ( rggbButton );
  rbox->addWidget ( bggrButton );
  rbox->addWidget ( grbgButton );
  rbox->addWidget ( gbrgButton );
  rbox->addWidget ( cmygButton );
  rbox->addWidget ( mcgyButton );
  rbox->addWidget ( ygcmButton );
  rbox->addWidget ( gymcButton );
  rbox->addWidget ( autoButton );
  rbox->addSpacing ( 15 );
  rbox->addWidget ( monoAsRaw );
  rbox->addStretch ( 1 );
  lbox->addWidget ( methodLabel );
  lbox->addWidget ( nnButton );
  lbox->addWidget ( bilinearButton );
  lbox->addWidget ( smoothHueButton );
  lbox->addWidget ( vngButton );
  lbox->addStretch ( 1 );
  hbox->addLayout ( lbox );
  hbox->addStretch ( 1 );
  hbox->addLayout ( rbox );
  hbox->addStretch ( 1 );
  setLayout ( hbox );

	if ( demosaicOpts ) {
    connect ( previewBox, SIGNAL ( stateChanged ( int )), parent,
        SLOT ( dataChanged()));
    connect ( outputBox, SIGNAL ( stateChanged ( int )), parent,
        SLOT ( dataChanged()));
	}
  connect ( cfaButtons, SIGNAL ( buttonClicked ( int )), parent,
      SLOT ( dataChanged()));
  connect ( methodButtons, SIGNAL ( buttonClicked ( int )), parent,
      SLOT ( dataChanged()));
  connect ( monoAsRaw, SIGNAL ( stateChanged ( int )), parent,
      SLOT ( dataChanged()));
}


DemosaicSettings::~DemosaicSettings()
{
  trampolines->destroyLayout (( QLayout* ) hbox );
}


void
DemosaicSettings::storeSettings ( void )
{
	if ( demosaicOpts ) {
    pconfig->demosaicPreview = previewBox->isChecked() ? 1 : 0;
    pconfig->demosaicOutput = outputBox->isChecked() ? 1 : 0;
	}
  if ( rggbButton->isChecked()) {
    pconfig->cfaPattern = OA_DEMOSAIC_RGGB;
  }
  if ( bggrButton->isChecked()) {
    pconfig->cfaPattern = OA_DEMOSAIC_BGGR;
  }
  if ( grbgButton->isChecked()) {
    pconfig->cfaPattern = OA_DEMOSAIC_GRBG;
  }
  if ( gbrgButton->isChecked()) {
    pconfig->cfaPattern = OA_DEMOSAIC_GBRG;
  }
  if ( cmygButton->isChecked()) {
    pconfig->cfaPattern = OA_DEMOSAIC_CMYG;
  }
  if ( mcgyButton->isChecked()) {
    pconfig->cfaPattern = OA_DEMOSAIC_MCGY;
  }
  if ( ygcmButton->isChecked()) {
    pconfig->cfaPattern = OA_DEMOSAIC_YGCM;
  }
  if ( gymcButton->isChecked()) {
    pconfig->cfaPattern = OA_DEMOSAIC_GYMC;
  }
  if ( autoButton->isChecked()) {
    pconfig->cfaPattern = OA_DEMOSAIC_AUTO;
  }
  if ( nnButton->isChecked()) {
    pconfig->demosaicMethod = OA_DEMOSAIC_NEAREST_NEIGHBOUR;
  }
  if ( bilinearButton->isChecked()) {
    pconfig->demosaicMethod = OA_DEMOSAIC_BILINEAR;
  }
  if ( smoothHueButton->isChecked()) {
    pconfig->demosaicMethod = OA_DEMOSAIC_SMOOTH_HUE;
  }
  if ( vngButton->isChecked()) {
    pconfig->demosaicMethod = OA_DEMOSAIC_VNG;
  }
  pconfig->monoIsRawColour = monoAsRaw->isChecked() ? 1 : 0;

  if ( trampolines->isCameraInitialised()) {
    int format = trampolines->videoFramePixelFormat();
		int enabled = trampolines->isDemosaicEnabled();
    trampolines->setVideoFramePixelFormat ( format );
		if ( demosaicOpts ) {
			trampolines->enableTIFFCapture (
					( !oaFrameFormats[ format ].rawColour ||
					( enabled && pconfig->demosaicOutput )) ? 1 : 0 );
			trampolines->enablePNGCapture (
					( !oaFrameFormats[ format ].rawColour ||
					( enabled && pconfig->demosaicOutput )) ? 1 : 0 );
			trampolines->enableMOVCapture (( QUICKTIME_OK( format ) || 
					( oaFrameFormats[ format ].rawColour && enabled &&
					pconfig->demosaicOutput )) ? 1 : 0 );
		}
  }
}


void
DemosaicSettings::updateCFASetting ( void )
{
  rggbButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_RGGB ? 1 : 0 );
  bggrButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_BGGR ? 1 : 0 );
  grbgButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_GRBG ? 1 : 0 );
  gbrgButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_GBRG ? 1 : 0 );
  cmygButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_CMYG ? 1 : 0 );
  mcgyButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_MCGY ? 1 : 0 );
  ygcmButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_YGCM ? 1 : 0 );
  gymcButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_GYMC ? 1 : 0 );
  autoButton->setChecked ( pconfig->cfaPattern == OA_DEMOSAIC_AUTO ? 1 : 0 );
}