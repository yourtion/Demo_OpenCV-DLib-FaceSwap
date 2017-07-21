//
//  main.cpp
//  FaceSwap
//
//  Created by YourtionGuo on 20/07/2017.
//  Copyright © 2017 Yourtion. All rights reserved.
//
//  c++ faceSwap.cpp  -o face -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_photo -lopencv_imgcodecs -ldlib -lopenblas
//  c++ faceSwap.cpp -O3  -o face -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_photo -lopencv_imgcodecs -ldlib -lopenblas
//

#include <iostream>
#include <vector>

#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_io.h>
#include <dlib/opencv/cv_image_abstract.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/photo.hpp>
#include <opencv2/highgui/highgui.hpp>

#ifdef DEBUG
  #include <dlib/image_processing/render_face_detections.h>
  #include <dlib/gui_widgets.h>
#endif

using namespace cv;
using namespace dlib;
using namespace std;

#ifdef DEBUG
  #define DLOG(fmt, ...) fprintf(stderr, fmt,__VA_ARGS__)
#else
  #define DLOG(fmt, ...)
#endif

struct correspondens{
  std::vector<int> index;
};


/*
 //detect 68 face landmarks on the input image by using the face landmark detector in dlib.
 */
void faceLandmarkDetection(dlib::array2d<unsigned char>& img, shape_predictor sp, std::vector<Point2f>& landmark)
{
  frontal_face_detector detector = get_frontal_face_detector();
  //dlib::pyramid_up(img);

  std::vector<dlib::rectangle> dets = detector(img);
  DLOG("Number of faces detected: %lu \n", dets.size());

  full_object_detection shape = sp(img, dets[0]);
#ifdef DEBUG
  image_window win;
  win.clear_overlay();
  win.set_image(img);
  win.add_overlay(render_face_detections(shape));
  win.wait_until_closed();
#endif

  for (int i = 0; i < shape.num_parts(); ++i)
  {
    float x=shape.part(i).x();
    float y=shape.part(i).y();
    landmark.push_back(Point2f(x,y));
  }

}


/*
 //perform Delaunay Triangulation on the keypoints of the convex hull.
 */

void delaunayTriangulation(const std::vector<Point2f>& hull,std::vector<correspondens>& delaunayTri,Rect rect)
{

  cv::Subdiv2D subdiv(rect);
  for (int it = 0; it < hull.size(); it++) {
    subdiv.insert(hull[it]);
  }

  DLOG("done subdiv add : %lu \n", hull.size());

  std::vector<Vec6f> triangleList;
  subdiv.getTriangleList(triangleList);
  DLOG("traingleList number is %lu \n", triangleList.size());

  //std::vector<Point2f> pt;
  //correspondens ind;
  for (size_t i = 0; i < triangleList.size(); ++i)
  {

    std::vector<Point2f> pt;
    correspondens ind;
    Vec6f t = triangleList[i];
    pt.push_back( Point2f(t[0], t[1]) );
    pt.push_back( Point2f(t[2], t[3]) );
    pt.push_back( Point2f(t[4], t[5]) );
    //cout<<"pt.size() is "<<pt.size()<<endl;

    if (rect.contains(pt[0]) && rect.contains(pt[1]) && rect.contains(pt[2]))
    {
      //cout<<t[0]<<" "<<t[1]<<" "<<t[2]<<" "<<t[3]<<" "<<t[4]<<" "<<t[5]<<endl;
      int count = 0;
      for (int j = 0; j < 3; ++j)
        for (size_t k = 0; k < hull.size(); k++)
          if (abs(pt[j].x - hull[k].x) < 1.0   &&  abs(pt[j].y - hull[k].y) < 1.0)
          {
            ind.index.push_back(k);
            count++;
          }
      if (count == 3)
        //cout<<"index is "<<ind.index[0]<<" "<<ind.index[1]<<" "<<ind.index[2]<<endl;
        delaunayTri.push_back(ind);
    }
    //pt.resize(0);
    //cout<<"delaunayTri.size is "<<delaunayTri.size()<<endl;
  }

}


// Apply affine transform calculated using srcTri and dstTri to src
void applyAffineTransform(Mat &warpImage, Mat &src, std::vector<Point2f> &srcTri, std::vector<Point2f> &dstTri)
{
  // Given a pair of triangles, find the affine transform.
  Mat warpMat = getAffineTransform( srcTri, dstTri );

  // Apply the Affine Transform just found to the src image
  warpAffine( src, warpImage, warpMat, warpImage.size(), cv::INTER_LINEAR, BORDER_REFLECT_101);
}

/*
 //morp a triangle in the one image to another image.
 */
void warpTriangle(Mat &img1, Mat &img2, std::vector<Point2f> &t1, std::vector<Point2f> &t2)
{

  Rect r1 = boundingRect(t1);
  Rect r2 = boundingRect(t2);

  // Offset points by left top corner of the respective rectangles
  std::vector<Point2f> t1Rect, t2Rect;
  std::vector<Point> t2RectInt;
  for(int i = 0; i < 3; i++)
  {

    t1Rect.push_back( Point2f( t1[i].x - r1.x, t1[i].y -  r1.y) );
    t2Rect.push_back( Point2f( t2[i].x - r2.x, t2[i].y - r2.y) );
    t2RectInt.push_back( Point(t2[i].x - r2.x, t2[i].y - r2.y) ); // for fillConvexPoly

  }

  // Get mask by filling triangle
  Mat mask = Mat::zeros(r2.height, r2.width, CV_32FC3);
  fillConvexPoly(mask, t2RectInt, Scalar(1.0, 1.0, 1.0), 16, 0);

  // Apply warpImage to small rectangular patches
  Mat img1Rect;
  img1(r1).copyTo(img1Rect);

  Mat img2Rect = Mat::zeros(r2.height, r2.width, img1Rect.type());

  applyAffineTransform(img2Rect, img1Rect, t1Rect, t2Rect);

  multiply(img2Rect,mask, img2Rect);
  multiply(img2(r2), Scalar(1.0,1.0,1.0) - mask, img2(r2));
  img2(r2) = img2(r2) + img2Rect;

}

void time_teack(int *start, const char *event) {
  printf("time - %s : %.0f ms\n", event,(clock() - *start) * 1000.0 / CLOCKS_PER_SEC );
  *start = clock();
}

int main(int argc, char** argv)
{
  int start = clock();
  bool v = false;
  if (argc < 3) {
    cout << "Give some image files as arguments to this program." << endl;
    return 0;
  }
  if (argc == 4) {
    v = true;
  }

  //----------------- step 1. load the input two images. ----------------------------------
  dlib::array2d<unsigned char> imgDlib1,imgDlib2;
  dlib::load_image(imgDlib1, argv[1]);
  dlib::load_image(imgDlib2, argv[2]);
  if(v) time_teack(&start, "load_image");

  Mat imgCV1 = imread(argv[1]);
  Mat imgCV2 = imread(argv[2]);
  if(!imgCV1.data || !imgCV2.data)
  {
    printf("No image data \n");
    return -1;
  }

  //---------------------- step 2. detect face landmarks -----------------------------------
  shape_predictor sp;
  deserialize("shape_predictor_68_face_landmarks.dat") >> sp;
  if(v) time_teack(&start, "deserialize");

  std::vector<Point2f> points1, points2;

  faceLandmarkDetection(imgDlib1,sp,points1);
  if(v) time_teack(&start, "faceLandmarkDetection1");
  faceLandmarkDetection(imgDlib2,sp,points2);
  if(v) time_teack(&start, "faceLandmarkDetection2");

  //---------------------step 3. find convex hull -------------------------------------------
  Mat imgCV1Warped = imgCV2.clone();
  imgCV1.convertTo(imgCV1, CV_32F);
  imgCV1Warped.convertTo(imgCV1Warped, CV_32F);
  if(v) time_teack(&start, "convertWarped");

  std::vector<Point2f> hull1;
  std::vector<Point2f> hull2;
  std::vector<int> hullIndex;

  cv::convexHull(points2, hullIndex, false, false);
  if(v) time_teack(&start, "convexHull");

  for(int i = 0; i < hullIndex.size(); i++)
  {
    hull1.push_back(points1[hullIndex[i]]);
    hull2.push_back(points2[hullIndex[i]]);
  }

  //-----------------------step 4. delaunay triangulation -------------------------------------
  std::vector<correspondens> delaunayTri;
  Rect rect(0, 0, imgCV1Warped.cols, imgCV1Warped.rows);
  delaunayTriangulation(hull2,delaunayTri,rect);
  if(v) time_teack(&start, "delaunayTriangulation");

  for(size_t i=0;i<delaunayTri.size();++i)
  {
    std::vector<Point2f> t1,t2;
    correspondens corpd=delaunayTri[i];
    for(size_t j=0;j<3;++j)
    {
      t1.push_back(hull1[corpd.index[j]]);
      t2.push_back(hull2[corpd.index[j]]);
    }

    warpTriangle(imgCV1,imgCV1Warped,t1,t2);
  }
  if(v) time_teack(&start, "warpTriangle");


  //------------------------step 5. clone seamlessly -----------------------------------------------

  //calculate mask
  std::vector<Point> hull8U;

  for(int i=0; i< hull2.size();++i)
  {
    Point pt(hull2[i].x,hull2[i].y);
    hull8U.push_back(pt);
  }


  Mat mask = Mat::zeros(imgCV2.rows,imgCV2.cols,imgCV2.depth());
  fillConvexPoly(mask, &hull8U[0], hull8U.size(), Scalar(255,255,255));
  if(v) time_teack(&start, "fillConvexPoly");


  Rect r = boundingRect(hull2);
  Point center = (r.tl() +r.br()) / 2;

  Mat output;
  imgCV1Warped.convertTo(imgCV1Warped, CV_8UC3);
  seamlessClone(imgCV1Warped,imgCV2,mask,center,output,NORMAL_CLONE);
  if(v) time_teack(&start, "seamlessClone");

  string filename=argv[1];
  filename= filename+argv[2];
  filename= filename +".jpg";
  imwrite(filename,output);
  if(v) time_teack(&start, "imwrite");


#ifdef DEBUG
  imshow("Face Swapped", output);
  waitKey(0);
  destroyAllWindows();
#endif
  
  return 0;
}
