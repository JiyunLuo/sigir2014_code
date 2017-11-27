import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.io.File;


public class evaluation {
	static String inFile = "testReal";
	static String truthFile = "qrel";
	static String outFile = "eval/run.eval";
	
	public static void main(String args[]){
              if(args.length <=0 ){
                System.out.println("evaluation inputFile");
              }
              inFile = args[0];
              File run = new File(inFile);
              outFile = "eval/" + run.getName();
                
		Map<Integer, Map<Integer, Integer>> mapRel = new HashMap<Integer, Map<Integer, Integer>>();	
		Map<Integer, Integer> tempMap = new HashMap<Integer, Integer>();
		String tempString, tempStringR;
		int sessionCount = 0;
		double DCG = 0;
		double nDCG = 0;
		double IDCG = 0;
		double precision = 0;
		int twoCount = 0;
		int oneCount = 0;
		
		try{
			BufferedReader br = new BufferedReader (new FileReader(inFile));
			BufferedReader brTruth = new BufferedReader (new FileReader(truthFile));
			BufferedWriter bw = new BufferedWriter (new FileWriter(outFile));
			System.out.println("Loading Ground Truth...");
			
			int line = 0;
			while ((tempStringR = brTruth.readLine()) != null){
				line++;
				if (line%1000000 == 0){
					System.out.println(line);
				}
				if (!mapRel.containsKey(Integer.valueOf(tempStringR.split("\t")[0]))){
					tempMap = new HashMap<Integer, Integer>();
					tempMap.put(Integer.valueOf(tempStringR.split("\t")[1]), Integer.valueOf(tempStringR.split("\t")[2]));
					mapRel.put(Integer.valueOf(tempStringR.split("\t")[0]), tempMap);
				}else{
					tempMap = mapRel.get(Integer.valueOf(tempStringR.split("\t")[0]));
					tempMap.put(Integer.valueOf(tempStringR.split("\t")[1]), Integer.valueOf(tempStringR.split("\t")[2]));
					mapRel.put(Integer.valueOf(tempStringR.split("\t")[0]), tempMap);
				}
			}
			System.out.println("Evaluating");
			double tempScore = 0;
			while ((tempString = br.readLine()) != null){
			
				DCG = 0;
				IDCG = 0;
				twoCount = 0;
				oneCount = 0;
				for (int i = 1; i <= 10; i++){
					tempScore = 0;
					if (mapRel.containsKey(Integer.valueOf(tempString.split(",")[0]))){
						if (mapRel.get(Integer.valueOf(tempString.split(",")[0])).containsKey(Integer.valueOf(tempString.split(",")[1]))){
							tempScore = mapRel.get(Integer.valueOf(tempString.split(",")[0])).get(Integer.valueOf(tempString.split(",")[1])).doubleValue();
						}
					}
					//System.out.println(tempString + "\t\tScore: " + tempScore);
					// Precision
					if (i == 1){
						if (tempScore > 0.1){
							precision += 1;
						}
					}
					// DCG
					if (tempScore > 0.1){
						if (tempScore > 1.9){
							tempScore = 3;
							twoCount++;
						}else if (tempScore > 0.9){
							tempScore = 1;
							oneCount++;
						}
						DCG += tempScore * Math.log1p(1) / Math.log1p(i);
					}
					if (i < 10){
						tempString = br.readLine();
					}
				}
				//IDCG
				for (int i = 1; i <= twoCount; i++){
					IDCG += (double)3 * Math.log1p(1) / Math.log1p(i);
				}
				for (int i = twoCount + 1; i <= twoCount+oneCount; i++){
					IDCG += (double)1 * Math.log1p(1) / Math.log1p(i);
				}
				//nDCG
				if (IDCG > 0){
					nDCG += (double) DCG/IDCG;
					sessionCount++;
				}
			}
			precision = (double) precision/sessionCount;
			nDCG = (double) nDCG/sessionCount;
			
			System.out.println("Evaluation Results:");
			System.out.println("Precision@1 = " + precision);
			System.out.println("nDCG@10 = " + nDCG);
			
			bw.write("Precision@1 = " + precision + "\n");
			bw.flush();
			bw.write("nDCG@10 = " + nDCG + "\n");
			bw.flush();
		}catch (IOException e){
			System.out.println("IOException!!");
		}
	}
}
