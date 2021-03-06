/*
 *  Copyright 2011 The SCAPE Project Consortium.
 * 
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *       http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *  under the License.
 */

package eu.scape_project.tb.mtr;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import java.util.TreeMap;
import org.apache.poi.hssf.usermodel.HSSFCell;
import org.apache.poi.hssf.usermodel.HSSFCellStyle;
import org.apache.poi.hssf.usermodel.HSSFDataFormat;
import org.apache.poi.hssf.usermodel.HSSFRow;
import org.apache.poi.hssf.usermodel.HSSFSheet;
import org.apache.poi.hssf.usermodel.HSSFWorkbook;
import org.apache.poi.hssf.util.HSSFColor;
import org.apache.poi.ss.format.CellFormat;
import org.apache.poi.ss.usermodel.CellStyle;
import org.apache.poi.ss.usermodel.DataFormat;

/**
 * This tool can be used to merge a set of CSV like files.
 * SCAPE FuE uses the tool to merge reports generated by TIFOWA or DROID into ONE combined XLS, CSV or JS report file. 
 *
 */
public class MergeReports 
{
    
    static HashMap<String, Integer> myCollection = new HashMap<String, Integer>();
    static int countAllGoodItems = 0;
     
    
    public static void main( String[] args ) throws Exception
    {
        String outputFilePath = "";
        String outputFormat = "";
        long processingTime = 0;
        System.out.println( "Usage: java -jar MergeReports.jar inputPath outputFileName outputformat" );
        System.out.println( "outputformat: csv, xls or js" );
        if(args.length > 2 && args.length < 4){
            System.out.println( "Passed Arguments:" );
            for (String s: args) System.out.println(s);
            String reportPath = args[0];
            outputFilePath = args[1];
            outputFormat = args[2];
            System.out.println("************************************");
            processingTime = createXLSfromCSV(reportPath);}
        else{
            System.out.println("Pass 3 arguments!");
        }
       
        
        diplayResult(processingTime);
        
        if(outputFormat.equals("xls")) writeXLS(outputFilePath);
        if(outputFormat.equals("csv")) writeCSV(outputFilePath);
        if(outputFormat.equals("js")) writeJS(outputFilePath);
        
    }

    private static long createXLSfromCSV(String reportPath) throws Exception {
        
        long startClock = System.currentTimeMillis();
        File path = new File(reportPath);
        
        if (path.isDirectory()) {
            String[] children = path.list();
            for (int i = 0; i < children.length; i++) {
                String inputFileFullPath = reportPath + File.separator + children[i];
                                
                if(new File(inputFileFullPath).isFile())
                {
                    int lastToken = inputFileFullPath.lastIndexOf(".");
                    String fileExtension = inputFileFullPath.substring(lastToken).toLowerCase();
                    System.out.println("Processing file: " + inputFileFullPath);
                    if(fileExtension.equals(".txt")) processInputFile(inputFileFullPath);
                    else System.out.println("   File skipped. Not a TXT file!");
                }
            }
        } else {
            System.out.println(path + " is NOT a directory!");
            throw new Exception(path + " is NOT a directory!");
        }
        
        
        
        long elapsedTimeMillis = System.currentTimeMillis()-startClock;
        return elapsedTimeMillis;
    }

    private static void processInputFile(String file) throws Exception {
        
  
        
        BufferedReader myFile = new BufferedReader(new FileReader(file));
        String dataRow = myFile.readLine();
        
        
        while (dataRow != null){
            String[] myTokens;
            //System.out.println("    ROW: " + dataRow);
            
            myTokens = dataRow.split("#");
            if(myTokens.length > 2 && !myTokens[0].toLowerCase().equals("type") && !myTokens[1].toLowerCase().equals("count"))
            { 
  
                String myType = myTokens[0];
                String myTypeCount = myTokens[1];
                
                if(myType.equals("")) myType = "NO_RESULT";
                
                //System.out.println("    EXTRACT: " + myType + " - " + myTypeCount);
                
                // Check for an existing key for the current type. Create it if it is not existing.
                try{int myGetCounter = myCollection.get(myType);}
                catch (Exception ex)
                { myCollection.put(myType, 0);}

                // Read the counter for the current type and increase the type counter 
                try{
                    myCollection.put(myType, myCollection.get(myType) + Integer.parseInt(myTypeCount));
                    countAllGoodItems = countAllGoodItems + Integer.parseInt(myTypeCount);
                }catch(Exception e)
                {
                    System.out.println("    ERROR! Wrong input file format: " + file);
                }
            }
            dataRow = myFile.readLine(); // Read next line
        }    
    }

    private static void diplayResult(long elapsedTimeSec) {
        
        System.out.println("************************************");
        System.out.println("Total file processing time (sec): " + elapsedTimeSec/1000F );
        System.out.println("************************************");
        System.out.println("Total number of files analyzed  : " + countAllGoodItems);
        System.out.println("************************************");
        System.out.println("*** You can import the data below into a CSV. Use # as the separator. ***");
        System.out.println();
        System.out.println("TYPE#COUNT#PERCENTAGE");
        Iterator it = myCollection.keySet().iterator();
        
        while(it.hasNext()){
            String typeKey = it.next().toString();
            float typeValue = myCollection.get(typeKey);
            float myPerc = typeValue/countAllGoodItems*100;
            System.out.println(typeKey + "#" + (int) typeValue + "#" + myPerc);
        }  
    }

    
    private static boolean writeJS(String outputFilePath) throws Exception {
        
        int rowCounter = 0; //count elememts to identify the last one
        
        FileWriter fw = new FileWriter(outputFilePath);
        PrintWriter pw = new PrintWriter(fw);


        
        pw.println("var json = {");
        pw.println("\t'label': ['MIMETYPE COUNT'],");
        pw.println("\t'values': [");
        
        Iterator it = myCollection.keySet().iterator();
        while(it.hasNext()){
 
            String typeKey = it.next().toString();
            int typeValue = myCollection.get(typeKey);
            //System.out.println("    ***: " + typeKey + "#" + (int) typeValue + "#" + myPerc);
            
            if(rowCounter != 0)pw.println("\t,"); // do not add "," for the last element
            pw.println("\t{");
            pw.println("\t\t'label': '" + typeKey + "',"); 
            pw.println("\t\t'values': [" + typeValue + "]");
            pw.println("\t}");
            
            rowCounter++;
            
        }  

        pw.println("\t]};");
        
        pw.flush();
        pw.close();
        fw.close();
        
        
        
        return true;
    }
    
    
    private static boolean writeCSV(String outputFilePath) throws Exception {
        
        int rowCounter = 1; //Start in row 1 (which is the 2nd row). Row 0 holds the description.
        
        FileWriter fw = new FileWriter(outputFilePath);
        PrintWriter pw = new PrintWriter(fw);

        pw.print("TYPE");
        pw.print(" ");
        pw.println("COUNT");
        
        Iterator it = myCollection.keySet().iterator();
        while(it.hasNext()){
            
            String typeKey = it.next().toString();
            int typeValue = myCollection.get(typeKey);
            //System.out.println("    ***: " + typeKey + "#" + (int) typeValue + "#" + myPerc);
            
            pw.print(typeKey);
            pw.print(" ");
            pw.println(typeValue);

            rowCounter++;
            
        }  

        
        pw.flush();
        pw.close();
        fw.close();
        
        
        
        return true;
    }
    
    
    private static boolean writeXLS(String outputFilePath) throws Exception {
        
        int rowCounter = 1; //Start in row 1 (which is the 2nd row). Row 0 holds the description.
        FileOutputStream fileOut = new FileOutputStream(outputFilePath);
        HSSFWorkbook workbook = new HSSFWorkbook();
	
        HSSFSheet worksheet = workbook.createSheet("Type Report");
        
        HSSFCellStyle myCellStyle = workbook.createCellStyle();
        myCellStyle.setAlignment(CellStyle.ALIGN_CENTER);
        
        HSSFCellStyle myCellStylePercent = workbook.createCellStyle();
        HSSFDataFormat df = workbook.createDataFormat();
        myCellStylePercent.setDataFormat(df.getFormat("0.00%"));
        myCellStylePercent.setAlignment(CellStyle.ALIGN_CENTER);
        
        HSSFRow myHeaderRow = worksheet.createRow(0);
        HSSFCell myHeaderCell0 = myHeaderRow.createCell(0);
        HSSFCell myHeaderCell1 = myHeaderRow.createCell(1);
        HSSFCell myHeaderCell2 = myHeaderRow.createCell(2);

        myHeaderCell0.setCellValue("TYPE");
        myHeaderCell1.setCellValue("COUNT");
        myHeaderCell2.setCellValue("PERCENTAGE");
        myHeaderCell0.setCellStyle(myCellStyle);
        myHeaderCell1.setCellStyle(myCellStyle);
        myHeaderCell2.setCellStyle(myCellStyle);

        Iterator it = myCollection.keySet().iterator();
        while(it.hasNext()){
            String typeKey = it.next().toString();
            float typeValue = myCollection.get(typeKey);
            float myPerc = typeValue/countAllGoodItems;
            //System.out.println("    ***: " + typeKey + "#" + (int) typeValue + "#" + myPerc);
            
            HSSFRow myRow = worksheet.createRow(rowCounter);
            HSSFCell myCell0 = myRow.createCell(0);
            HSSFCell myCell1 = myRow.createCell(1);
            HSSFCell myCell2 = myRow.createCell(2);
            
            myCell0.setCellValue(typeKey);
            myCell1.setCellValue(typeValue);
            myCell2.setCellValue(myPerc);
            

            myCell1.setCellStyle(myCellStyle);
            myCell2.setCellStyle(myCellStyle);
            
            myCell2.setCellStyle(myCellStylePercent);

            rowCounter++;
            
        }  

        
        workbook.write(fileOut);
        fileOut.flush();
        fileOut.close();
        
        
        
        return true;
    }
}

