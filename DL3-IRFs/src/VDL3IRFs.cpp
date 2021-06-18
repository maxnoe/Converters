#include "VDL3IRFs.h"

VDL3IRFs::VDL3IRFs()
{
    fptr = 0;
}


/*
 * fits error reporting
*/
bool VDL3IRFs::printerror( int status )
{
    if( status )
    {
        fits_report_error( stderr, status );
    }
    return false;
}


bool VDL3IRFs::open_fits_file( string fits_file_name )
{
    int status = 0;
    if( fits_create_file( &fptr, fits_file_name.c_str(), &status ) )
    {
        return printerror( status );
    }
    if( fits_create_img( fptr,  DOUBLE_IMG, 0, 0, &status ) )
    {
        return printerror( status );
    }

    return true;
}

bool VDL3IRFs::write_fits_file()
{
    int status = 0;
    if( fits_close_file( fptr, &status ) )
    {
         return printerror( status );
    }
    return true;
}

bool VDL3IRFs::write_fits_header()
{
   int status = 0;

   char author[] = "Eventdisplay";
   if( fits_update_key( fptr, TSTRING, ( char* )"CREATOR", author, ( char* )"Creator", &status ) )
   {
       return printerror( status );
   }
   char telescope[] = "CTA (MC prod5)";
   if( fits_update_key( fptr, TSTRING, ( char* )"TELESCOP", telescope, ( char* )"Telescope name", &status ) )
   {
       return printerror( status );
   }
   return true;
}

bool VDL3IRFs::write_fits_keyword( 
                    char *key_name,
                    char *key_value,
                    char *key_comment )
{
   int status = 0;
   if( fits_update_key( fptr, 
                        TSTRING, 
                        key_name, 
                        key_value,
                        key_comment,
                        &status ) )
   {
       return printerror( status );
   }
   return true;
}

bool VDL3IRFs::write_fits_table_header( string irftype )
{
   cout << "write_fits_table_header " << irftype << endl;

   write_fits_keyword( (char*)"TELESCOP",
                       (char*)"CTA",
                       (char*)"Name of telescope" );

   // date string
   time_t now;
   time(&now);
   char buf[sizeof "2011-10-08T07:07:09"];
   strftime(buf, sizeof buf, "%FT%T", gmtime(&now));
   write_fits_keyword( (char*)"DATE",
                       buf,
                       (char*)"File creation date (YYYY-MM-DDThh:mm:ss UTC)" );


   write_fits_keyword( (char*)"HDUDOC",
                       (char*)"https://github.com/open-gamma-ray-astro/gamma-astro-data-formats",
                       (char*)"" );

   write_fits_keyword( (char*)"HDUVERS",
                       (char*)"0.2",
                       (char*)"HDU version" );

   write_fits_keyword( (char*)"HDUCLASS",
                       (char*)"GADF",
                       (char*)"HDUCLASS" );

   write_fits_keyword( (char*)"HDUCLAS1",
                       (char*)"RESPONSE",
                       (char*)"HDUCLAS1" );

   if( irftype == "PSF_3GAUSS" )
   {
       write_fits_keyword( (char*)"HDUCLAS2",
                           (char*)"PSF",
                           (char*)"HDUCLAS1" );
   }
   else if( irftype == "BKG_2D" )
   {
       write_fits_keyword( (char*)"HDUCLAS2",
                           (char*)"BKG",
                           (char*)"HDUCLAS1" );
   }
   else if( irftype == "AEFF_2D" )
   {
       write_fits_keyword( (char*)"HDUCLAS2",
                           (char*)"EFF_AREA",
                           (char*)"HDUCLAS1" );
   }
   else if( irftype == "EDISP_2D" )
   {
       write_fits_keyword( (char*)"HDUCLAS2",
                           (char*)"EDISP",
                           (char*)"HDUCLAS1" );
   }

   write_fits_keyword( (char*)"HDUCLAS3",
                       (char*)"FULL-ENCLOSURE",
                       (char*)"HDUCLAS3" );

   write_fits_keyword( (char*)"HDUCLAS4",
                       (char*)irftype.c_str(),
                       (char*)"HDUCLAS4" );

   return true;
}

/*
   normalise rows of fixed E_true and off-axis bins
   to 1 (pdf)
*/
void VDL3IRFs::normalise_pdf( TH3F *h )
{
    if( !h ) return;

    // off-axis bins (fix)
    for( int j = 0; j < h->GetNbinsZ(); j++ )
    {
        // true energy axis bins (fix)
        for( int i = 0; i < h->GetNbinsX(); i++ )
        {
            // energy dispersion: normalise to one
            double sum = 0.;
            for( int k = 0; k < h->GetNbinsY(); k++ )
            {
                sum += h->GetBinContent( i+1, k+1, j+1 );
            }
            // require at least then entries
            // to be useful
            if( sum > 10. )
            {
                for( int k = 0; k < h->GetNbinsY(); k++ )
                {
                    h->SetBinContent( i+1, k+1, j+1,
                              h->GetBinContent(  i+1, k+1, j+1 ) / sum );
                }
            }
            // otherwise set all bins to zero
            else
            {
                for( int k = 0; k < h->GetNbinsY(); k++ )
                {
                    h->SetBinContent( i+1, k+1, j+1, 0. );
                }
            }
        }
    }
}

/*
 * Write energy dispersion
 * https://gamma-astro-data-formats.readthedocs.io/en/latest/irfs/full_enclosure/edisp/index.html
 *
*/
bool VDL3IRFs::write_edisp( TH3F *h )
{
   if( !h ) return false;

   // make sure that PDfs are normalised
   // (this is not guaranteed by Eventdisplay)
   normalise_pdf( h );

   int status = 0;

   const int nCol = 7;
   long nRows = h->GetNbinsX() 
              * h->GetNbinsZ() 
              * h->GetNbinsY();
   nRows = 0;
   char* tType[nCol] = { (char*)"ENERG_LO",
                      (char*)"ENERG_HI",
                      (char*)"MIGRA_LO",
                      (char*)"MIGRA_HI",
                      (char*)"THETA_LO",
                      (char*)"THETA_HI",
                      (char*)"MATRIX"};
   char* tUnit[nCol] = { (char*)"TeV",
                      (char*)"TeV",
                      (char*)"",
                      (char*)"",
                      (char*)"deg",
                      (char*)"deg",
                      (char*)""};
   // e_true axis is x-axis
   char x_form[10];
   sprintf( x_form, "%dE", h->GetNbinsX() );
   // e_mig = e_reco / e_true
   char y_form[10];
   sprintf( y_form, "%dE", h->GetNbinsY() );
   // offset angle axis is z-axis
   char z_form[10];
   sprintf( z_form, "%dE", h->GetNbinsZ() );
   // mig
   char m_form[10];
   sprintf( m_form, "%dE", h->GetNbinsX()
                         * h->GetNbinsZ()
                         * h->GetNbinsY() );
   char* tForm[nCol] = { &x_form[0],
                      &x_form[0],
                      &y_form[0],
                      &y_form[0],
                      &z_form[0],
                      &z_form[0],
                      &m_form[0] };
   ///////////////
   // create empty table
   if( fits_create_tbl( fptr, 
                        BINARY_TBL, 
                        nRows, 
                        nCol, 
                        tType, 
                        tForm, 
                        tUnit, 
                        "ENERGY DISPERSION",
                        &status ) )
   {
       return printerror( status );
   }
   // set dimensions
   long int naxes[] = { h->GetNbinsX(), 
                        h->GetNbinsY(), 
                        h->GetNbinsZ() };
   if( fits_write_tdim( fptr,
                        7,
                        3,
                        naxes,
                        &status ) )
   {
      return printerror( status );
   }

   ///////////////
   // write data 
   vector< vector< float > > table = get_baseline_axes( h );

   vector< float > data;
   for( int k = 0; k < h->GetNbinsZ(); k++ )
   {
       for( int j = 0; j < h->GetNbinsY(); j++ )
       {
           for( int i = 0; i < h->GetNbinsX(); i++ )
           {
               data.push_back( h->GetBinContent( i+1, j+1, k+1 ) );
           }
       }
   }
   table.push_back( data );

   bool writing_success = write_table( table );
   write_fits_table_header( "EDISP_2D" );

   return writing_success;
}

/*
 *  Multi-Gauss mixture model
 *  https://gamma-astro-data-formats.readthedocs.io/en/latest/irfs/full_enclosure/psf/psf_3gauss/index.html#
*/
bool VDL3IRFs::write_psf_gauss( TH2F *h )
{
   if( !h ) return false;

   int status = 0;
   const int nCol = 10;
   long nRows = h->GetNbinsX() * h->GetNbinsY();
   nRows = 0;
   char* tType[nCol] = { (char*)"ENERG_LO",
                      (char*)"ENERG_HI",
                      (char*)"THETA_LO",
                      (char*)"THETA_HI",
                      (char*)"SCALE",
                      (char*)"SIGMA_1",
                      (char*)"SIGMA_2",
                      (char*)"SIGMA_3",
                      (char*)"AMPL_2",
                      (char*)"AMPL_3" };
   char* tUnit[nCol] = { (char*)"TeV",
                      (char*)"TeV",
                      (char*)"deg",
                      (char*)"deg",
                      (char*)"sr^(-1)",
                      (char*)"deg",
                      (char*)"deg",
                      (char*)"deg",
                      (char*)"",
                      (char*)"" };
   char x_form[10];
   sprintf( x_form, "%dE", h->GetNbinsX() );
   char y_form[10];
   sprintf( y_form, "%dE", h->GetNbinsY() );
   char z_form[10];
   sprintf( z_form, "%dE", h->GetNbinsX()*h->GetNbinsY() );
   char* tForm[nCol] = { &x_form[0],
                      &x_form[0],
                      &y_form[0],
                      &y_form[0],
                      &z_form[0],
                      &z_form[0],
                      &z_form[0],
                      &z_form[0],
                      &z_form[0],
                      &z_form[0] };
   ///////////////
   // create empty table
   if( fits_create_tbl( fptr, 
                        BINARY_TBL, 
                        nRows, 
                        nCol, 
                        tType, 
                        tForm, 
                        tUnit, 
                        "POINT SPREAD FUNCTION",
                        &status ) )
   {
       return printerror( status );
   }
   // set dimensions
   long int naxes[] = { h->GetNbinsX(),  h->GetNbinsY() };
   int dim_colnum[] = { 5, 6, 7, 8, 9, 10 };
   for( int i = 0; i < 6; i++ )
   {
       if( fits_write_tdim( fptr,
                            dim_colnum[i],
                            2,
                            naxes,
                            &status ) )
      {
          return printerror( status );
      }
   }
   ///////////////
   // write data 
   vector< vector< float > > table = get_baseline_axes( h );

   // data
   vector< float > data;
   vector< float > scale;
   vector< float > empty;
   for( int j = 0; j < h->GetNbinsY(); j++ )
   {
       for( int i = 0; i < h->GetNbinsX(); i++ )
      {
          // convert 68% to 2D sigma
          data.push_back( h->GetBinContent( i+1, j+1 ) * 0.6624305 );
          if( data.back() > 0. )
          {
             scale.push_back( 1./ (2.*TMath::Pi()*data.back() ) );
          }
          else
          {
              scale.push_back( 0. );
          }
          empty.push_back( 0. );
      }
   }
   table.push_back( scale );
   table.push_back( data );
   for( unsigned int i = 0; i < 4; i++ )
   {
       table.push_back( empty );
   }

   bool writing_success = write_table( table );
   write_fits_table_header( "PSF_3GAUSS" );

   return writing_success;
}

/*
 * background IRF
 * - https://github.com/open-gamma-ray-astro/gamma-astro-data-formats/issues/153
 */
bool VDL3IRFs::write_background( TH2F *h )
{
   bool writing_success = write_histo2D( h,
                      "BACKGROUND",
                      (char*)"BKG",
                      (char*)"s^-1 MeV^-1 sr^-1",
                      true );
   write_fits_table_header( "BKG_2D" );
   return writing_success;
}

/*
 * effective area IRF
 * - https://gamma-astro-data-formats.readthedocs.io/en/latest/irfs/full_enclosure/aeff/index.html
 */
bool VDL3IRFs::write_effarea( TH2F *h )
{
   bool writing_success = write_histo2D( h,
                      "EFFECTIVE AREA",
                      (char*)"EFFAREA",
                      (char*)"m**2",
                      false );
   write_fits_table_header( "AEFF_2D" );
   return writing_success;
}

bool VDL3IRFs::write_histo2D( TH2F *h,
                               string name,
                               char* col_name,
                               char* col_unit,
                               bool MEV_BACKGROUND_UNIT )
{
   if( !h ) return false;

   int status = 0;
   const int nCol = 5;
   long nRows = h->GetNbinsX() * h->GetNbinsY();
   nRows = 0;
   char* tType[nCol] = { (char*)"ENERG_LO",
                      (char*)"ENERG_HI",
                      (char*)"THETA_LO",
                      (char*)"THETA_HI",
                      (char*)col_name };
   char* tUnit[nCol] = { (char*)"TeV",
                      (char*)"TeV",
                      (char*)"deg",
                      (char*)"deg",
                      (char*)col_unit };
   char x_form[10];
   sprintf( x_form, "%dE", h->GetNbinsX() );
   char y_form[10];
   sprintf( y_form, "%dE", h->GetNbinsY() );
   char z_form[10];
   sprintf( z_form, "%dE", h->GetNbinsX()*h->GetNbinsY() );
   char* tForm[nCol] = { &x_form[0],
                      &x_form[0],
                      &y_form[0],
                      &y_form[0],
                      &z_form[0] };

   ///////////////
   // create empty table
   if( fits_create_tbl( fptr, 
                        BINARY_TBL, 
                        nRows, 
                        nCol, 
                        tType, 
                        tForm, 
                        tUnit, 
                        name.c_str() , 
                        &status ) )
   {
       return printerror( status );
   }
   // set dimensions
   long int naxes[] = { h->GetNbinsX(),  h->GetNbinsY() };
   if( fits_write_tdim( fptr,
                        5,
                        2,
                        naxes,
                        &status ) )
   {
      return printerror( status );
   }
   ///////////////
   // write data 
   vector< vector< float > > table = get_baseline_axes( h );
   // coordinate conversion from 
   // 1/deg^2/s --> 1/sr^2/s
   vector< float > norm_mev_background( h->GetNbinsX(), 1. );
   if( MEV_BACKGROUND_UNIT )
   {
      float dE = 1.;
      for( unsigned int i = 0; i < norm_mev_background.size(); i++ )
      {
          norm_mev_background[i] /= TMath::DegToRad() * TMath::DegToRad();
          // first two axes are E low and high
          // TeV --> MeV
          if( table.size() > 1 
          && i < table[0].size() 
          && i < table[1].size() )
          {
             dE = table[1][i] - table[0][i];
             dE *= 1.e6;
             norm_mev_background[i] /= dE;
          }
      }
   }

   // data
   vector< float > data;
   for( int j = 0; j < h->GetNbinsY(); j++ )
   {
      for( int i = 0; i < h->GetNbinsX(); i++ )
      {
          // background normalisation
          // (all vector sizes checked)
          if( MEV_BACKGROUND_UNIT )
          {
              data.push_back( h->GetBinContent( i+1, j+1 ) 
                             * norm_mev_background[i] );
          }
          else
          {
              data.push_back( h->GetBinContent( i+1, j+1 ) );
          }

      }
   }
   table.push_back( data );

   return write_table( table );
}


bool VDL3IRFs::write_table( vector< vector< float > > table )
{
   int status = 0;

   for( unsigned int i = 0; i < table.size(); i++ )
   {
       cout << "\t writing column " << i+1 << "\t" << table[i].size() << endl;
       if( fits_write_col( fptr, 
                       TFLOAT,
                       i+1,
                       1,
                       1,
                       table[i].size(),
                       &table[i][0],
                       &status ) )
       {
           return printerror( status );
       }
   }

   return true;
}

/*
  get baseline axes with
  upper or lower bin edge of a histogram axis
*/
vector<vector<float>> VDL3IRFs::get_baseline_axes( TH1 *h )
{
   vector< vector< float > > table;
   if( !h ) return table;

   // xaxis
   // (always expected to be energy axis in log10)
   vector< float > xedge_low;
   vector< float > xedge_hig;
   for( int i = 0; i < h->GetNbinsX(); i++ )
   {
       xedge_low.push_back( TMath::Power( 10.,
                                         h->GetXaxis()->GetBinLowEdge( i+1 ) ) );
       xedge_hig.push_back( TMath::Power( 10.,
                                         h->GetXaxis()->GetBinUpEdge( i+1 ) ) );
   }
   table.push_back( xedge_low );
   table.push_back( xedge_hig);
   // yaxis
   vector< float > yedge_low;
   vector< float > yedge_hig;
   for( int i = 0; i < h->GetNbinsY(); i++ )
   {
       yedge_low.push_back( h->GetYaxis()->GetBinLowEdge( i+1 ) );
       yedge_hig.push_back( h->GetYaxis()->GetBinUpEdge( i+1 ) );
   }
   table.push_back( yedge_low );
   table.push_back( yedge_hig);

   if( h->GetDimension() == 3 )
   {
       // zaxis
       vector< float > zedge_low;
       vector< float > zedge_hig;
       for( int i = 0; i < h->GetNbinsZ(); i++ )
       {
           zedge_low.push_back( h->GetZaxis()->GetBinLowEdge( i+1 ) );
           zedge_hig.push_back( h->GetZaxis()->GetBinUpEdge( i+1 ) );
       }
       table.push_back( zedge_low );
       table.push_back( zedge_hig);
   }

   return table;
}

