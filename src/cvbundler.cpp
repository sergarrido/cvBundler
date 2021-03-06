#include "cvbundler.h"

#include <fstream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include "BundlerApp.h"

namespace cvbundler {

void _writeMatchesFileVSFM(const vector< vector< MatchIndexList > > &matchesIndexes) {
    ofstream matchesfile;
    matchesfile.open("vsfmmatches.txt");
    for(int i=0; i<matchesIndexes.size()-1; i++) {
        for(int j=i+1; j<matchesIndexes.size(); j++) {
            if(matchesIndexes[i][j].size() == 0) continue;
            matchesfile << "img" << i << ".jpg img" << j << ".jpg " << matchesIndexes[i][j].size() << "\n";
            for(int k=0; k<matchesIndexes[i][j].size(); k++)
                matchesfile << matchesIndexes[i][j][k].first << " ";
            matchesfile << "\n";
            for(int k=0; k<matchesIndexes[i][j].size(); k++)
                matchesfile << matchesIndexes[i][j][k].second << " ";
            matchesfile << "\n";
        }
    }
    matchesfile.close();
}


void cvbundler::doStructureFromMotion(const vector<Mat> &images, const vector< KeyPointList > &keypoints,
                                      const vector< vector< MatchIndexList > > &matchesIndexes)
{
    CV_Assert(images.size() >= 2);
    CV_Assert(images.size() == keypoints.size());
    CV_Assert(images.size() == matchesIndexes.size());

    // Bundler interface is a bit confusing, some of the data is read from files in the middle of the process.
    // Thus, instead of pass my data directly, the simplest option is creating the files myself and make Bundler read them.

    _writeImagesFiles("scene3_imageList.txt", "scene3_img", images);

    _writeKeypointsFiles("scene3_img", keypoints);

    _matchesFilename = "scene3_matches.txt";
    _writeMatchesFile(_matchesFilename, matchesIndexes);
    _writeMatchesFileVSFM(matchesIndexes);

    string outputFilename = "scene3_bundle.out";
    _writeOptionsFile("scene3_options.txt", outputFilename);

    if(_fixedIntrinsic) _writeIntrinsicFile("scene3_intrinsics.txt");

    std::cerr << "GENERATING DATA FOR BUNDLER, CHANGE X Y IN KEYPOINTS FOR VSFM" << std::endl;

    char** argv;
    int argc;
    _getargcargv("imageList", "./options.txt", argc, argv);

    BundlerApp* bundler_app = new BundlerApp();
    bundler_app->argc = argc;
    bundler_app->argv = argv;

    return;

    bundler_app->OnInit();

    readOutputFile(outputFilename);

}




void cvbundler::_writeImagesFiles(string imgListFilename, string imageBasename, const vector<Mat> &images) {
    ofstream imgListFile;
    imgListFile.open(imgListFilename.c_str());
    for(int i=0; i<images.size(); i++) {
        stringstream imgName;
        imgName << imageBasename << i << ".jpg";
        imwrite(imgName.str(), images[i]);
        if(_fixedIntrinsic)
            imgListFile << "./" << imgName.str() << " 0 " << 0.5*(_camMatrix[i].ptr<float>()[0]+_camMatrix[i].ptr<float>()[4])  << "\n";
        else
            imgListFile << "./" << imgName.str() << "\n";
    }
    imgListFile.close();
}

void cvbundler::_writeKeypointsFiles(string basename, const vector< KeyPointList > &keypoints) {
    for(int i=0; i<keypoints.size(); i++) {
        stringstream filename;
        filename << basename << i << ".key";
        ofstream keyfile;
        keyfile.open(filename.str().c_str());
        keyfile << keypoints[i].size() << " 128\n";
        for(int k=0; k<keypoints[i].size(); k++) {
            keyfile << keypoints[i][k].x << " " << keypoints[i][k].y << " 1.0 1.0\n";
            for(int k=0; k<6; k++) keyfile << "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\n";
            keyfile << "1 1 1 1 1 1 1 1\n";
        }
        keyfile.close();
    }
}

void cvbundler::_writeMatchesFile(string matchesFilename, const vector< vector< MatchIndexList > > &matchesIndexes) {
    ofstream matchesfile;
    matchesfile.open(matchesFilename.c_str());
    for(int i=0; i<matchesIndexes.size()-1; i++) {
        for(int j=i+1; j<matchesIndexes.size(); j++) {
            if(matchesIndexes[i][j].size() == 0) continue;
            matchesfile << i << " " << j << "\n";
            matchesfile << matchesIndexes[i][j].size() << "\n";
            for(int k=0; k<matchesIndexes[i][j].size(); k++)
                matchesfile << matchesIndexes[i][j][k].first << " " << matchesIndexes[i][j][k].second << "\n";
        }
    }
    matchesfile.close();
}


void cvbundler::_writeOptionsFile(string optionsFilename, string outputFilename) {
    ofstream optionsfile;
    optionsfile.open(optionsFilename.c_str());
    optionsfile << "--run_bundle\n";
    //optionsfile << "--slow_bundle\n";
    optionsfile << "--match_table " << _matchesFilename << "\n";
    optionsfile << "--output " << outputFilename << "\n";
    if(false && _fixedIntrinsic) {
        optionsfile << "--intrinsics intrinsics.txt\n";
    }
    else {
        optionsfile << "--constrain_focal\n";
        optionsfile << "--use_focal_estimate\n";
        optionsfile << "--estimate_distortion\n";
        optionsfile << "--variable_focal_length\n";
        optionsfile << "--constrain_focal_weight 0.0001\n";
    }
    optionsfile.close();
}


void cvbundler::_writeIntrinsicFile(string intrinsicsFilename) {
    ofstream intrinsicsfile;
    intrinsicsfile.open(intrinsicsFilename.c_str());
    intrinsicsfile << _camMatrix.size() << "\n";
    for(int c=0; c<_camMatrix.size(); c++) {
        CV_Assert(_camMatrix[c].total() == 9);
        CV_Assert(_distCoeffs[c].total() <= 5);

        Mat aux;

        _camMatrix[c].convertTo(aux, CV_64FC1);
        for(int i=0; i<aux.total(); i++)
            intrinsicsfile << aux.ptr<double>()[i] << " ";
        intrinsicsfile << "\n";

        _distCoeffs[c].convertTo(aux, CV_64FC1);
        for(int i=0; i<aux.total(); i++)
            intrinsicsfile << aux.ptr<double>()[i] << " ";
        for(int i=aux.total(); i<5; i++)
            intrinsicsfile << "0.0 ";
        intrinsicsfile << "\n";
    }
    intrinsicsfile.close();
}


void cvbundler::_getargcargv(string imgListFilename, string optionsFilename, int &argc, char** &argv) {
    argv = new char* [4];

    argv[0] = strdup("bundler");
    argv[1] = strdup(imgListFilename.c_str());
    argv[2] = strdup("--options_file");
    argv[3] = strdup(optionsFilename.c_str());

    argc = 4;
}


void cvbundler::readOutputFile(string filename) {
    ifstream outputFile;
    outputFile.open(filename.c_str());
    std::string word;

    getline(outputFile, word); // get header line
    outputFile >> word;
    int nCameras = atoi(word.c_str());
    std::cout << "Number of cameras" << nCameras << std::endl;
    outputFile >> word;
    int nPoints = atoi(word.c_str());
    std::cout << "Number of points" << nPoints << std::endl;

    _rvecs.resize(nCameras);
    _tvecs.resize(nCameras);

    for(int c=0; c<nCameras; c++) {
        std::cout << "Cam " << c << std::endl;
        outputFile >> word;
        std::cout << "Focal length" << atof(word.c_str()) << std::endl;
        outputFile >> word;
        std::cout << "Dist 1 " << atof(word.c_str()) << std::endl;
        outputFile >> word;
        std::cout << "Dist 2" << atof(word.c_str()) << std::endl;

        cv::Mat rotMatrix(3, 3, CV_32FC1);
        for(int i=0; i<9; i++) {
            outputFile >> word;
            rotMatrix.ptr<float>()[i] = atof(word.c_str());
        }
        cv::Rodrigues(rotMatrix, _rvecs[c]);

        _tvecs[c] = Mat(3, 1, CV_32FC1);
        for(int i=0; i<3; i++) {
            outputFile >> word;
            _tvecs[c].ptr<float>()[i] = atof(word.c_str());
        }
        std::cout << _tvecs[c] << std::endl;
    }

    _points3d.resize(nPoints);
    _pointsColor.resize(nPoints);
    for(int p=0; p<nPoints; p++) {
        outputFile >> word;
        _points3d[p].x = atof(word.c_str());
        outputFile >> word;
        _points3d[p].y = atof(word.c_str());
        outputFile >> word;
        _points3d[p].z = atof(word.c_str());
        outputFile >> word;
        _pointsColor[p].val[0] = atoi(word.c_str());
        outputFile >> word;
        _pointsColor[p].val[1] = atoi(word.c_str());
        outputFile >> word;
        _pointsColor[p].val[2] = atoi(word.c_str());
        outputFile >> word;
        getline(outputFile, word); // ignore view list line
    }

    outputFile.close();
}



}
