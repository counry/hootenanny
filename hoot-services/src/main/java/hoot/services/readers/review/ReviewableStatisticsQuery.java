/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. DigitalGlobe
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2015, 2016 DigitalGlobe (http://www.digitalglobe.com/)
 */
package hoot.services.readers.review;

import static hoot.services.models.db.QCurrentRelations.currentRelations;

import java.sql.Connection;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.querydsl.core.types.dsl.Expressions;
import com.querydsl.sql.SQLQuery;

import hoot.services.models.review.ReviewQueryMapper;
import hoot.services.models.review.ReviewableStatistics;
import hoot.services.utils.DbUtils;


class ReviewableStatisticsQuery extends ReviewableQueryBase implements IReviewableQuery {
    private static final Logger logger = LoggerFactory.getLogger(ReviewableStatisticsQuery.class);

    ReviewableStatisticsQuery(Connection connection, long mapId) {
        super(connection, mapId);
    }

    private long getTotalReviewablesCount() {
        //String sql = "select count(*) as totalcnt from current_relations_" + getMapId() + " where tags->'type' = 'review'";

        return new SQLQuery<>(super.getConnection(), DbUtils.getConfiguration(super.getMapId()))
                .select()
                .from(currentRelations)
                .where(Expressions.booleanTemplate("tags->'type' = 'review'"))
                .fetchCount();
    }

    private long getRemainingReviewablesCount() {
        //String sql = "select count(*) as remaining from current_relations_" + getMapId() + " where tags->'hoot:review:needs' = 'yes'";

        return new SQLQuery<>(super.getConnection(), DbUtils.getConfiguration(super.getMapId()))
                .select()
                .from(currentRelations)
                .where(Expressions.booleanTemplate("tags->'hoot:review:needs' = 'yes'"))
                .fetchCount();
    }

    @Override
    public ReviewQueryMapper execQuery() {
        long totalCount = this.getTotalReviewablesCount();
        long remainingCount = this.getRemainingReviewablesCount();
        return new ReviewableStatistics(totalCount, remainingCount);
    }
}
