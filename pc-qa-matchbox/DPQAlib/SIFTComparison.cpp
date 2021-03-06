#include "SIFTComparison.h"

const string SIFTComparison::TASK_NAME = "SIFTComparison";

TCLAP::ValueArg<int> argSDK       ("","sdk"  ,      "[SIFTComparison] Number of Spatial Distinctive Keypoints (0 = no SDK)",                   false,0 ,"int");
TCLAP::ValueArg<int> argCLAHE     ("","clahe",      "[SIFTComparison] Value of adaptive contrast enhancement (1 = no enhancement)",            false,1 ,"int");
TCLAP::ValueArg<int> argPRECLUSTER("","precluster", "[SIFTComparison] Number of descriptors to select in preclustering (0 = no preclustering)",false,0 ,"int");
TCLAP::ValueArg<int> argDOWNSAMPLE("","downsample", "[SIFTComparison] Sample the image down to this resolution"                               ,false,1000000 ,"int");

SIFTComparison::SIFTComparison(void):Level3Feature()
{
	name          = TASK_NAME;
	scale         = 1;

	dispersion    = -1;
	uniformity    = -1;
	sizeVariation = -1;

	addCharacterizationCommandlineArgument(&argSDK);
	addCharacterizationCommandlineArgument(&argCLAHE);
	addCharacterizationCommandlineArgument(&argPRECLUSTER);
	addCharacterizationCommandlineArgument(&argDOWNSAMPLE);

	addComparisonCommandlineArgument(&argSDK);
}

SIFTComparison::~SIFTComparison(void)
{
	descriptors.release();
	keypoints.clear();
}

vector<int> SIFTComparison::findSpatiallyDistinctiveLocalKeypoints(Mat& image, vector<KeyPoint>& keypoints)
{
	vector<int>      results_index;

	int vert_lines        = int(sqrt(double((sdk*image.cols)/image.rows)));
	int hor_lines         = sdk / vert_lines;

	int hor_raster_width  = image.rows / hor_lines;
	int vert_raster_width = image.cols / vert_lines;

	int x                 = 0;
	int y                 = 0;

	/// Detector parameters
	Mat    dst, img_gray, dst_norm, dst_norm_scaled;
	int    blockSize      = 2;
	int    apertureSize   = 3;
	double kc             = 0.04;

	cvtColor( image, img_gray, CV_BGR2GRAY );
	cornerHarris( img_gray, dst, blockSize, apertureSize, kc, BORDER_DEFAULT );
	normalize( dst, dst_norm, 0, 255, NORM_MINMAX, CV_32FC1, Mat() );
	convertScaleAbs( dst_norm, dst_norm_scaled );
	
	while((x < image.cols) || (y < image.rows))
	{
		int x_min      = x;
		int x_max      = x_min + vert_raster_width;

		int y_min      = y;
		int y_max      = y_min + hor_raster_width;

		int curr_x_min = 0;
		int curr_x_max = curr_x_min + vert_raster_width;

		x = x_max;
		y = y_max;

		while(curr_x_max < image.cols)
		{
			KeyPoint kp_max;
			int      kp_max_index = -1;
			float    val          = 0;
			bool     kpSet        = false;
			int      idx          = 0;

			for (vector<KeyPoint>::iterator it = keypoints.begin(); it!=keypoints.end(); ++it) {

				KeyPoint kp = *it;

				if (((kp.pt.x >= curr_x_min) && (kp.pt.x <= curr_x_max)) && ((kp.pt.y >= y_min) && (kp.pt.y <= y_max)))
				{	
					float* p = dst_norm_scaled.ptr<float>(kp.pt.y);

					if (val < p[int(kp.pt.x)])
					{
						val          = p[int(kp.pt.x)];
						kp_max       = kp;
						kp_max_index = idx;
						kpSet        = true;
					}
				}

				idx++;
			}

			if (kpSet)
			{
				results_index.push_back(kp_max_index);
			}

			curr_x_min = curr_x_max;
			curr_x_max = curr_x_min + vert_raster_width;
		}
	}

	return results_index;
}


void SIFTComparison::execute(Mat& img)
{
	Feature::verbosePrintln(string("extracting features"));

	image = img.clone();

	// too big images may fail to be processed due to resource limitations (e.g. internal memory)
	if ((img.rows * img.cols) > maxResolution)
	{
		// downsample image to approx. MAX_IMAGE_RESOLUTION
		Feature::verbosePrintln(string("downsampling image"));
		image = downsample(image);
		
	}

	if (clahe != 1)
	{
		Feature::verbosePrintln(string("CLAHE"));

		Feature::verbosePrintln(string("Convert image to grayscale"));
		cvtColor( image, image, CV_RGB2GRAY );

		IplImage ipl_src  = image;
		Mat      dest     = image.clone();
		IplImage ipl_dest = dest;

		cvCLAdaptEqualize(&ipl_src, &ipl_dest,16,16,256,clahe, CV_CLAHE_RANGE_FULL);

		Mat imgMat(&ipl_dest);
		image = imgMat;

		Feature::verbosePrintln(string("Convert image back to RGB"));
		cvtColor( image, image, CV_GRAY2BGR );
	}

	
	try
	{
		vector<KeyPoint> kps;

		// extract features
		Feature::verbosePrintln(string("detecting keypoints"));
		Ptr<SIFT> featureDetector = new SIFT();
		
		Mat base;
		
		// workaround for SIFT-Feature detection bug in OpenCV Version 2.4.2
		resize(image, base, Size(), 2, 2, INTER_CUBIC); // remove this line if bug is resolved
		featureDetector->detect( base, kps );
		for (int i=0;i<kps.size();i++) {                // remove this line if bug is resolved
			kps[i].pt.x = kps[i].pt.x/2;                // remove this line if bug is resolved
			kps[i].pt.y = kps[i].pt.y/2;                // remove this line if bug is resolved
			kps[i].size = kps[i].size/2;                // remove this line if bug is resolved
		}                                               // remove this line if bug is resolved

		
		if (sdk != 0)
		{
			 vector<int> indeces = findSpatiallyDistinctiveLocalKeypoints(image, kps);
			 keypoints           = filterKeypoints(kps,indeces);
		}
		else
		{
			keypoints = kps;
		}

		if (keypoints.size() > 0)
		{
			dispersion    = calcDispersion( keypoints, image );
			uniformity    = calcUniformity( keypoints );
			sizeVariation = calcSizeVariation( keypoints );
		}

		// calculate descriptors
		Ptr<SiftDescriptorExtractor> descriptorExtractor = new SiftDescriptorExtractor();
		Feature::verbosePrintln(string("computing descriptors"));
		descriptorExtractor->compute( image, keypoints, descriptors );
	
		normalizeDescriptors(descriptors);

		precluster(numClusterCenters);

		Feature::verbosePrintln(string("Feature extraction done!"));
	}
	catch (Exception& ex)
	{

		stringstream msg;
		msg << "Error while extracting SIFT features: " << ex.msg;
		throw runtime_error(msg.str());
	}
}

void SIFTComparison::precluster( int clusterCenters )
{	
	if ((clusterCenters > 0) && (descriptors.rows > clusterCenters))
	{
		Feature::verbosePrintln(string("preclustering"));
		Mat          labels;
		TermCriteria tcrit;
		int          attempts = 3;
		Mat          centers;

		kmeans(descriptors,clusterCenters,labels,tcrit,3, KMEANS_PP_CENTERS, centers);
		descriptors = centers;
	}

}

void SIFTComparison::compare(Feature *task)
{
	try
	{
		Feature::verbosePrintln(string("comparing"));

		vector<KeyPoint> keypointsTrain = keypoints;
		vector<KeyPoint> keypointsQuery = ((SIFTComparison*)task)->getKeypoints();

		Mat descriptorsTrain = descriptors;
		Mat descriptorsQuery = ((SIFTComparison*)task)->getDescriptors();

		verbosePrintln(string("match descriptors"));

		// find matching keypoints
		RobustMatcher  matcher;
		vector<DMatch> matches;

		stringstream ss;
		ss << "keypointsQuery: "     << keypointsQuery.size() << ", keypointsTrain: "   << keypointsTrain.size();
		ss << ", descriptorsQuery: " << descriptorsQuery.rows << ", descriptorsTrain: " << descriptorsTrain.rows;
		verbosePrintln(ss.str());

		// load images
		verbosePrintln(string("load images"));
		Mat matImageTrainOrig = imread(const_cast<char*>(filename.c_str()), CV_LOAD_IMAGE_COLOR);
		Mat matImageQueryOrig = imread(const_cast<char*>(task->getFilename().c_str()), CV_LOAD_IMAGE_COLOR );

		if (sdk != 0)
		{
			verbosePrintln(string("Searching for Spatially Distinctive Local Keypoints"));
			vector<int> indecesOrig  = findSpatiallyDistinctiveLocalKeypoints(matImageQueryOrig, keypointsQuery);
			keypointsQuery           = filterKeypoints   (keypointsQuery,   indecesOrig);
			descriptorsQuery         = filterDescriptors (descriptorsQuery, indecesOrig);

			vector<int> indecesTrain = findSpatiallyDistinctiveLocalKeypoints(matImageTrainOrig, keypointsTrain);
			keypointsTrain           = filterKeypoints   (keypointsTrain,   indecesTrain);
			descriptorsTrain         = filterDescriptors (descriptorsTrain, indecesTrain);

			stringstream ss;
			ss << "keypointsQuery: "     << keypointsQuery.size() << ", keypointsTrain: "   << keypointsTrain.size();
			ss << ", descriptorsQuery: " << descriptorsQuery.rows << ", descriptorsTrain: " << descriptorsTrain.rows;
			verbosePrintln(ss.str());
		}

		matcher.match(keypointsQuery, keypointsTrain, descriptorsQuery, descriptorsTrain, matches );

		verbosePrintln("number of matches = " + StringConverter::toString((int)matches.size()));

		Mat affineTransform = calcAffineTransform(matches, keypointsTrain, keypointsQuery, ((SIFTComparison*)task)->getScale());

		

		

		verbosePrintln(string("warp"));

		Mat dst(matImageTrainOrig.size(),matImageTrainOrig.type());
		warpAffine(matImageQueryOrig,dst,affineTransform,matImageTrainOrig.size(),INTER_LINEAR,BORDER_CONSTANT,0);

		double ssim = SSIM::calcSSIM(matImageTrainOrig, dst,-1);

		DoubleOutputParameter *ssimParam = new DoubleOutputParameter("ssim");
		ssimParam->setData(ssim);
		addOutputParameter(*ssimParam);

		// create mask
		Mat dstMask(matImageTrainOrig.size(),CV_8U);
		Mat queryMask = Mat::ones(matImageQueryOrig.size(),CV_8U);

		warpAffine(queryMask,dstMask,affineTransform,matImageTrainOrig.size(),INTER_NEAREST,BORDER_CONSTANT,0);

		double ssimMasked = SSIM::calcSSIM(matImageTrainOrig, dst,-1,CV_BGR2YUV, dstMask);

		DoubleOutputParameter *ssimMaskedParam = new DoubleOutputParameter("ssimMasked");
		ssimMaskedParam->setData(ssimMasked);
		addOutputParameter(*ssimMaskedParam);

	}
	catch (exception e)
	{
		ErrorOutputParameter* error = new ErrorOutputParameter();
		error->setErrorMessage(e.what());
		addOutputParameter(*error);
	}
}

Mat SIFTComparison::calcAffineTransform(vector<DMatch>& matches, vector<KeyPoint>& keypointsTrain, vector<KeyPoint>& keypointsQuery, double scale2)
{

	// Build a
	Mat a((matches.size() * 2), 6, CV_64F);
	Mat b((matches.size() * 2), 1, CV_64F);

	// foreach
	int j = 0;

	for(int i = 0; i < matches.size(); ++i)
	{		
		DMatch match = matches[i];

		Mat M1 = (Mat_<double>(1,6) << keypointsTrain[match.trainIdx].pt.x, keypointsTrain[match.trainIdx].pt.y, 0, 0, 1, 0);
		M1.copyTo(a.row(j));

		M1.release();

		b.at<double>(j,0) = keypointsQuery[match.queryIdx].pt.x;

		j++;

		Mat M2 = (Mat_<double>(1,6) << 0, 0, keypointsTrain[match.trainIdx].pt.x, keypointsTrain[match.trainIdx].pt.y, 0, 1);
		M2.copyTo(a.row(j));

		M2.release();

		b.at<double>(j,0) = keypointsQuery[match.queryIdx].pt.y;

		j++;
	}

	// calculate transform
	Mat at = a.t() * a;
	at = at.inv() * a.t() * b;

	// re-arrange matrix
	Mat M(2,3,CV_64F);

	// translation matrix
	M.at<double>(0,0) = at.at<double>(0,0);
	M.at<double>(0,1) = at.at<double>(1,0);
	M.at<double>(1,0) = at.at<double>(2,0);
	M.at<double>(1,1) = at.at<double>(3,0);

	// affine matrix
	M.at<double>(0,2) = at.at<double>(4,0);
	M.at<double>(1,2) = at.at<double>(5,0);

	invertAffineTransform(M, M);

	M.at<double>(0,0) = M.at<double>(0,0) / (scale / scale2);
	M.at<double>(0,1) = M.at<double>(0,1) / (scale / scale2);
	M.at<double>(1,0) = M.at<double>(1,0) / (scale / scale2);
	M.at<double>(1,1) = M.at<double>(1,1) / (scale / scale2);

	M.at<double>(0,2) = M.at<double>(0,2) / scale;
	M.at<double>(1,2) = M.at<double>(1,2) / scale;

	return M;
}

Mat SIFTComparison::downsample(Mat& matImg)
{
	Mat orImg;

	try
	{
		// calculate scaling factor
		scale = 1/sqrt((double)((matImg.rows * matImg.cols) / maxResolution));
		resize(matImg,orImg, Size(), scale, scale,INTER_LINEAR);
	}
	catch(Exception& e)
	{
		stringstream msg;
		msg << "Error while downsampling image: " << e.msg;
		throw runtime_error(msg.str());
	}
		
	return orImg;
}

void SIFTComparison::writeOutput(FileStorage& fs)
{
	try
	{
		verbosePrintln(string("writing data"));

		fs << TASK_NAME << "{";
		
		write(fs, "keypoints", keypoints);
		write(fs, "descriptors", descriptors);
		
		fs << "scale" << scale;
		fs << "filename" << filename;

		if (dispersion > 0)
		{
			fs << "dispersion" << dispersion;
			fs << "uniformity" << uniformity;
			fs << "sizeVariation" << sizeVariation;
		}
		
		fs << "}";
	}
	catch (Exception& e)
	{
		stringstream msg;
		msg << "Error while writing SIFT data: " << e.msg;
		throw runtime_error(msg.str());
	}
}

void SIFTComparison::readData(FileNode& fs)
{
	read(fs["keypoints"], keypoints);
	if (keypoints.size() == 0)
	{
		throw runtime_error("Error while reading SIFTCompairison data: No Keypoints read!");
	}

	read(fs["descriptors"], descriptors);
	if ((descriptors.rows == 0) || (descriptors.cols == 0))
	{
		throw runtime_error("Error while reading SIFTCompairison data: No Descriptors read!");
	}

	fs["scale"] >> scale;
	if ((scale <= 0) || (scale > 1))
	{
		throw runtime_error("Error while reading SIFTCompairison data: Scale out of range!");
	}

	fs["filename"] >> filename;
	if (filename.size() == 0)
	{
		throw runtime_error("Error while reading SIFTCompairison data: Empty filename!");
	}
}

list<string>* SIFTComparison::getCmdlineArguments()
{
	list<string>* result = new list<string>;
	result->push_back(StringConverter::toString(sdk));
	return result;
}

void SIFTComparison::setCmdlineArguments(list<string> *args)
{
	sdk = StringConverter::toInt(&args->front());
}

void SIFTComparison::parseCommandlineArguments()
{
	sdk               = argSDK.getValue();
	clahe             = argCLAHE.getValue();
	numClusterCenters = argPRECLUSTER.getValue();
	maxResolution     = argDOWNSAMPLE.getValue();
}

Mat& SIFTComparison::getImage(void)
{
	return image;
}

Mat SIFTComparison::getDescriptors(void)
{
	return descriptors;
}

vector<KeyPoint> SIFTComparison::getKeypoints(void)
{
	return keypoints;
}

double SIFTComparison::getScale(void)
{
	return scale;
}

double SIFTComparison::calcDispersion( vector<KeyPoint>& keypoints, Mat& image )
{

	Feature::verbosePrintln(string("calculate dispersion"));
	// initialize variables
	double h        = 0;
	double weight   = 4.79129;

	// calculate 2D-Histogram of Keypoint-Coordinates
	Mat kp_map = Mat::zeros(image.rows, image.cols, CV_32F);

	for(vector<KeyPoint>::iterator it = keypoints.begin(); it != keypoints.end(); it++)
	{
		kp_map.at<float>((*it).pt) += 1;		
	}

	// calculate dispersion
	for (int scale = 2; scale <= 64; scale *= 2)
	{
		double h_s        = 0;
		double m_expected = keypoints.size() / (scale * scale);

		int partWidth;
		int partHeight;

		int    partWidthFloor  = floor(((float)image.cols / (float)scale));
		int    partHeightFloor = floor(((float)image.rows / (float)scale));
		int    partWidthCeil = partWidthFloor + 1;
		int    partHeightCeil = partHeightFloor + 1;
		int    errorWidth = image.cols - partWidthFloor*scale;
		int    errorHeight = image.rows - partHeightFloor*scale;

		int    y_start    = 0;

		// for each row
		for (int i = 0; i < scale; i++)
		{
			int x_start = 0;

			// Distribute the error in Height across errorHeight subMatrices
			partHeight = partHeightCeil;
			if (i > (errorHeight - 1))
			{
				partHeight = partHeightFloor; 
			}

			// for each line
			for (int j = 0; j < scale; j++)
			{
				Rect subMatrix;

				// Distribute the error in Width across errorWidth subMatrices
				partWidth = partWidthCeil;   
				if (j > (errorWidth - 1))
				{
					partWidth = partWidthFloor;
				}

				subMatrix = Rect(x_start,y_start,partWidth,partHeight);

				// retrieve the number of keypoints for this segment
				Scalar m_observed = sum(kp_map(subMatrix));

				// calculate h_s for this segment
				h_s += fabs(m_observed.val[0] - m_expected) / (2 * keypoints.size());

				// update start-coordinate
				x_start += partWidth;
			}


			// update start-coordinate
			y_start += partHeight;
		}

		// calculate h_s for this scale
		h += std::pow(weight,(1-(log10((double)scale)/log10((double)2)))) * h_s;
	}

	return h;
}

bool compare_angle(KeyPoint first, KeyPoint second)
{
	if (first.angle < second.angle)
		return true;
	else 
		return false;
}

double SIFTComparison::calcUniformity( vector<KeyPoint>& keypoints )
{
	Feature::verbosePrintln(string("calculate uniformity"));

	// angle uniformity as used in Rao test for circular uniformity. 
	// Rao, J.S. (1969). Some contributions to the analysis of circular data. Ph.D. thesis, Indian Statistical Institute, Calcutta.
	// http://www.pstat.ucsb.edu/faculty/jammalam/html/favorite/test.htm

	// initialize variables
	double u        = 0;
	double lambda	= 360.0/(double)keypoints.size();
	double ti	= 0;
	//double rad2angle = 180/3.14159265;
	
	vector<KeyPoint>::iterator it;

	sort( keypoints.begin(), keypoints.end(), compare_angle );

    double prevAngle = keypoints[ keypoints.size()-1 ].angle - 360.0;

	for(vector<KeyPoint>::iterator it = keypoints.begin()+1; it != keypoints.end(); it++)
	{
		KeyPoint kp = *it;
		ti = ( kp.angle  - prevAngle ); // * rad2angle;
		prevAngle = kp.angle;
		u += fabs( ti - lambda);
	}

	//ti = ( keypoints[0].angle - keypoints[ keypoints.size()-1].angle ); // * rad2angle;
	//u += fabs ( ti - lambda );

	u = u/2;

	return u;
}

double SIFTComparison::calcSizeVariation( vector<KeyPoint>& keypoints )
{
	Feature::verbosePrintln(string("calculate size variation"));

	// scale/size variance 

	// initialize variables
	double var      = 0;
	double mean		= 0;

	for(vector<KeyPoint>::iterator it = keypoints.begin(); it != keypoints.end()-1; it++)
	{
		KeyPoint kp = *it;
		mean += kp.size;
	}
	mean = mean / (double)keypoints.size();

	for(vector<KeyPoint>::iterator it = keypoints.begin(); it != keypoints.end()-1; it++)
	{
		KeyPoint kp = *it;
		var += powf( kp.size - mean, 2.0 ) ;
	}
	var = var / (double)keypoints.size();

	return var;
}

void SIFTComparison::normalizeDescriptors( Mat& descriptors )
{
	for (int i = 0; i < descriptors.rows; i++)
	{
		Mat row = descriptors.row(i);

		double sqrtOfSum = std::sqrt(sum(row)[0]);

		for (int j = 0; j < 127; j++)
		{
			row.at<float>(0,j) = row.at<float>(0,j) / sqrtOfSum;
		}
	}
}

vector<KeyPoint> SIFTComparison::filterKeypoints( vector<KeyPoint>& origKeypoints, vector<int>& indeces )
{
	vector<KeyPoint> result;

	for (vector<int>::iterator it = indeces.begin(); it!=indeces.end(); ++it)
	{
		result.push_back(keypoints.at((*it)));
	}

	return result;
}

cv::Mat SIFTComparison::filterDescriptors( Mat& origDescriptors, vector<int>& indeces )
{
	Mat result(indeces.size(), origDescriptors.cols, origDescriptors.type());

	int idx = 0;

	for (vector<int>::iterator it = indeces.begin(); it!=indeces.end(); ++it)
	{
		result.row(idx) = origDescriptors.row((*it));
		idx++;
	}

	return result;

}
