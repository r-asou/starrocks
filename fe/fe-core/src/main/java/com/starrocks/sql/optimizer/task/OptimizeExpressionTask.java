// This file is licensed under the Elastic License 2.0. Copyright 2021-present, StarRocks Limited.

package com.starrocks.sql.optimizer.task;

import com.google.common.collect.Lists;
import com.starrocks.sql.optimizer.GroupExpression;
import com.starrocks.sql.optimizer.rule.Rule;
import com.starrocks.sql.optimizer.rule.RuleType;

import java.util.Comparator;
import java.util.List;

/**
 * If exploreOnly is true:
 * OptimizeExpressionTask explores a GroupExpression by constructing all logical
 * transformations and applying those rules.
 * <p>
 * If exploreOnly is false:
 * OptimizeExpressionTask optimizes a GroupExpression by constructing all logical
 * and physical transformations and applying those rules.
 * <p>
 * The rules are sorted by promise and applied in that order
 * so a physical transformation rule is applied before a logical transformation rule.
 */
public class OptimizeExpressionTask extends OptimizerTask {
    private final boolean exploreOnly;
    private final GroupExpression groupExpression;

    OptimizeExpressionTask(TaskContext context, GroupExpression expression, boolean exploreOnly) {
        super(context);
        this.groupExpression = expression;
        this.exploreOnly = exploreOnly;
    }

    private List<Rule> getValidRules() {
        List<Rule> validRules = Lists.newArrayListWithCapacity(RuleType.NUM_RULES.id());
        List<Rule> logicalRules = context.getOptimizerContext().getRuleSet().
                getTransformRules();
        filterInValidRules(groupExpression, logicalRules, validRules);

        if (!exploreOnly) {
            List<Rule> physicalRules = context.getOptimizerContext().getRuleSet().
                    getImplementRules();
            filterInValidRules(groupExpression, physicalRules, validRules);
        }
        return validRules;
    }

    @Override
    public String toString() {
        return "OptimizeExpressionTask for groupExpression " + groupExpression +
                "\n exploreOnly " + exploreOnly;
    }

    @Override
    public void execute() {
        List<Rule> rules = getValidRules();
        rules.sort(Comparator.comparingInt(Rule::promise));

        for (Rule rule : rules) {
            pushTask(new ApplyRuleTask(context, groupExpression, rule, exploreOnly));
        }
    }
}
